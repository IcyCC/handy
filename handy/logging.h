#pragma once
#include <stdio.h>
#include <atomic>
#include <string>
#include "util.h"

#ifdef NDEBUG
#define hlog(level, ...)                                                                \
    do {                                                                                \
        if (level <= Logger::getLogger().getLogLevel()) {                               \
            Logger::getLogger().logv(level, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        }                                                                               \
    } while (0)
#else
#define hlog(level, ...)                                                                \
    do {                                                                                \
        if (level <= Logger::getLogger().getLogLevel()) {                               \
            snprintf(0, 0, __VA_ARGS__);                                                \
            Logger::getLogger().logv(level, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        }                                                                               \
    } while (0)

#endif

#define handy_trace_log(...) hlog(Logger::LTRACE, __VA_ARGS__)
#define handy_debug_log(...) hlog(Logger::LDEBUG, __VA_ARGS__)
#define handy_info_log(...) hlog(Logger::LINFO, __VA_ARGS__)
#define handy_warn_log(...) hlog(Logger::LWARN, __VA_ARGS__)
#define handy_error_log(...) hlog(Logger::LERROR, __VA_ARGS__)
#define handy_fatal_log(...) hlog(Logger::LFATAL, __VA_ARGS__)
#define handy_fatalif_log(b, ...)                        \
    do {                                       \
        if ((b)) {                             \
            hlog(Logger::LFATAL, __VA_ARGS__); \
        }                                      \
    } while (0)
#define handy_check_log(b, ...)                          \
    do {                                       \
        if ((b)) {                             \
            hlog(Logger::LFATAL, __VA_ARGS__); \
        }                                      \
    } while (0)
#define handy_exitif_log(b, ...)                         \
    do {                                       \
        if ((b)) {                             \
            hlog(Logger::LERROR, __VA_ARGS__); \
            _exit(1);                          \
        }                                      \
    } while (0)

#define handy_setloglevel(l) Logger::getLogger().setLogLevel(l)
#define handy_setlogfile(n) Logger::getLogger().setFileName(n)

namespace handy {

struct Logger : private noncopyable {
    enum LogLevel { LFATAL = 0, LERROR, LUERR, LWARN, LINFO, LDEBUG, LTRACE, LALL };
    Logger();
    ~Logger();
    void logv(int level, const char *file, int line, const char *func, const char *fmt...);

    void setFileName(const std::string &filename);
    void setLogLevel(const std::string &level);
    void setLogLevel(LogLevel level) { level_ = std::min(LALL, std::max(LFATAL, level)); }

    LogLevel getLogLevel() { return level_; }
    const char *getLogLevelStr() { return levelStrs_[level_]; }
    int getFd() { return fd_; }

    void adjustLogLevel(int adjust) { setLogLevel(LogLevel(level_ + adjust)); }
    void setRotateInterval(long rotateInterval) { rotateInterval_ = rotateInterval; }
    static Logger &getLogger();

   private:
    void maybeRotate();
    static const char *levelStrs_[LALL + 1];
    int fd_;
    LogLevel level_;
    long lastRotate_;
    std::atomic<int64_t> realRotate_;
    long rotateInterval_;
    std::string filename_;
};

}  // namespace handy
