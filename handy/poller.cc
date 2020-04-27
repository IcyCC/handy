#include <fcntl.h>
#include "conn.h"
#include "event_base.h"
#include "logging.h"
#include "util.h"
#include "poller.h"

#ifdef OS_LINUX
#include <sys/epoll.h>
#elif defined(OS_MACOSX)
#include <sys/event.h>
#else
#error "platform unsupported"
#endif

namespace handy {

#ifdef OS_LINUX

struct PollerEpoll : public PollerBase {
    int fd_;
    std::set<Channel *> liveChannels_;
    // for epoll selected active events
    struct epoll_event activeEvs_[kMaxEvents];
    PollerEpoll();
    ~PollerEpoll();
    void addChannel(Channel *ch) override;
    void removeChannel(Channel *ch) override;
    void updateChannel(Channel *ch) override;
    void loop_once(int waitMs) override;
};

PollerBase *createPoller() {
    return new PollerEpoll();
}

PollerEpoll::PollerEpoll() {
    fd_ = epoll_create1(EPOLL_CLOEXEC);
    handy_fatalif_log(fd_ < 0, "epoll_create error %d %s", errno, strerror(errno));
    handy_info_log("poller epoll %d created", fd_);
}

PollerEpoll::~PollerEpoll() {
    handy_info_log("destroying poller %d", fd_);
    while (liveChannels_.size()) {
        (*liveChannels_.begin())->close();
    }
    ::close(fd_);
    handy_info_log("poller %d destroyed", fd_);
}

void PollerEpoll::addChannel(Channel *ch) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = ch->events();
    ev.data.ptr = ch;
    handy_trace_log("adding channel %lld fd %d events %d epoll %d", (long long) ch->id(), ch->fd(), ev.events, fd_);
    int r = epoll_ctl(fd_, EPOLL_CTL_ADD, ch->fd(), &ev);
    handy_fatalif_log(r, "epoll_ctl add failed %d %s", errno, strerror(errno));
    liveChannels_.insert(ch);
}

void PollerEpoll::updateChannel(Channel *ch) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = ch->events();
    ev.data.ptr = ch;
    handy_trace_log("modifying channel %lld fd %d events read %d write %d epoll %d", (long long) ch->id(), ch->fd(), ev.events & POLLIN, ev.events & POLLOUT, fd_);
    int r = epoll_ctl(fd_, EPOLL_CTL_MOD, ch->fd(), &ev);
    handy_fatalif_log(r, "epoll_ctl mod failed %d %s", errno, strerror(errno));
}

void PollerEpoll::removeChannel(Channel *ch) {
    handy_trace_log("deleting channel %lld fd %d epoll %d", (long long) ch->id(), ch->fd(), fd_);
    liveChannels_.erase(ch);
    for (int i = lastActive_; i >= 0; i--) {
        if (ch == activeEvs_[i].data.ptr) {
            activeEvs_[i].data.ptr = NULL;
            break;
        }
    }
}

void PollerEpoll::loop_once(int waitMs) {
    int64_t ticks = util::timeMilli();
    lastActive_ = epoll_wait(fd_, activeEvs_, kMaxEvents, waitMs);
    int64_t used = util::timeMilli() - ticks;
    handy_trace_log("epoll wait %d return %d errno %d used %lld millsecond", waitMs, lastActive_, errno, (long long) used);
    handy_fatalif_log(lastActive_ == -1 && errno != EINTR, "epoll return error %d %s", errno, strerror(errno));
    while (--lastActive_ >= 0) {
        int i = lastActive_;
        Channel *ch = (Channel *) activeEvs_[i].data.ptr;
        int events = activeEvs_[i].events;
        if (ch) {
            if (events & (kReadEvent | POLLERR)) {
                handy_trace_log("channel %lld fd %d handle read", (long long) ch->id(), ch->fd());
                ch->handleRead();
            } else if (events & kWriteEvent) {
                handy_trace_log("channel %lld fd %d handle write", (long long) ch->id(), ch->fd());
                ch->handleWrite();
            } else {
                handy_fatal_log("unexpected poller events");
            }
        }
    }
}

#elif defined(OS_MACOSX)

struct PollerKqueue : public PollerBase {
    int fd_;
    std::set<Channel *> liveChannels_;
    // for epoll selected active events
    struct kevent activeEvs_[kMaxEvents];
    PollerKqueue();
    ~PollerKqueue();
    void addChannel(Channel *ch) override;
    void removeChannel(Channel *ch) override;
    void updateChannel(Channel *ch) override;
    void loop_once(int waitMs) override;
};

PollerBase *createPoller() {
    return new PollerKqueue();
}

PollerKqueue::PollerKqueue() {
    fd_ = kqueue();
    handy_fatalif_log(fd_ < 0, "kqueue error %d %s", errno, strerror(errno));
    handy_info_log("poller kqueue %d created", fd_);
}

PollerKqueue::~PollerKqueue() {
    handy_info_log("destroying poller %d", fd_);
    while (liveChannels_.size()) {
        (*liveChannels_.begin())->close();
    }
    ::close(fd_);
    handy_info_log("poller %d destroyed", fd_);
}

void PollerKqueue::addChannel(Channel *ch) {
    struct timespec now;
    now.tv_nsec = 0;
    now.tv_sec = 0;
    struct kevent ev[2];
    int n = 0;
    if (ch->readEnabled()) {
        EV_SET(&ev[n++], ch->fd(), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, ch);
    }
    if (ch->writeEnabled()) {
        EV_SET(&ev[n++], ch->fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, ch);
    }
    handy_trace_log("adding channel %lld fd %d events read %d write %d  epoll %d", (long long) ch->id(), ch->fd(), ch->events() & POLLIN, ch->events() & POLLOUT, fd_);
    int r = kevent(fd_, ev, n, NULL, 0, &now);
    handy_fatalif_log(r, "kevent add failed %d %s", errno, strerror(errno));
    liveChannels_.insert(ch);
}

void PollerKqueue::updateChannel(Channel *ch) {
    struct timespec now;
    now.tv_nsec = 0;
    now.tv_sec = 0;
    struct kevent ev[2];
    int n = 0;
    if (ch->readEnabled()) {
        EV_SET(&ev[n++], ch->fd(), EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, ch);
    } else {
        EV_SET(&ev[n++], ch->fd(), EVFILT_READ, EV_DELETE, 0, 0, ch);
    }
    if (ch->writeEnabled()) {
        EV_SET(&ev[n++], ch->fd(), EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, ch);
    } else {
        EV_SET(&ev[n++], ch->fd(), EVFILT_WRITE, EV_DELETE, 0, 0, ch);
    }
    handy_trace_log("modifying channel %lld fd %d events read %d write %d epoll %d", (long long) ch->id(), ch->fd(), ch->events() & POLLIN, ch->events() & POLLOUT, fd_);
    int r = kevent(fd_, ev, n, NULL, 0, &now);
    handy_fatalif_log(r, "kevent mod failed %d %s", errno, strerror(errno));
}

void PollerKqueue::removeChannel(Channel *ch) {
    handy_trace_log("deleting channel %lld fd %d epoll %d", (long long) ch->id(), ch->fd(), fd_);
    liveChannels_.erase(ch);
    // remove channel if in ready stat
    for (int i = lastActive_; i >= 0; i--) {
        if (ch == activeEvs_[i].udata) {
            activeEvs_[i].udata = NULL;
            break;
        }
    }
}

void PollerKqueue::loop_once(int waitMs) {
    struct timespec timeout;
    timeout.tv_sec = waitMs / 1000;
    timeout.tv_nsec = (waitMs % 1000) * 1000 * 1000;
    long ticks = util::timeMilli();
    lastActive_ = kevent(fd_, NULL, 0, activeEvs_, kMaxEvents, &timeout);
    handy_trace_log("kevent wait %d return %d errno %d used %lld millsecond", waitMs, lastActive_, errno, util::timeMilli() - ticks);
    handy_fatalif_log(lastActive_ == -1 && errno != EINTR, "kevent return error %d %s", errno, strerror(errno));
    while (--lastActive_ >= 0) {
        int i = lastActive_;
        Channel *ch = (Channel *) activeEvs_[i].udata;
        struct kevent &ke = activeEvs_[i];
        if (ch) {
            // only handle write if read and write are enabled
            if (!(ke.flags & EV_EOF) && ch->writeEnabled()) {
                handy_trace_log("channel %lld fd %d handle write", (long long) ch->id(), ch->fd());
                ch->handleWrite();
            } else if ((ke.flags & EV_EOF) || ch->readEnabled()) {
                handy_trace_log("channel %lld fd %d handle read", (long long) ch->id(), ch->fd());
                ch->handleRead();
            } else {
                handy_fatal_log("unexpected epoll events %d", ch->events());
            }
        }
    }
}

#endif
}  // namespace handy