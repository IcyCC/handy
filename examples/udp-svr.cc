// echo server
#include <handy/handy.h>
using namespace handy;

int main(int argc, const char *argv[]) {
    handy_setloglevel("TRACE");
    EventBase base;
    Signal::signal(SIGINT, [&] { base.exit(); });
    UdpServerPtr svr = UdpServer::startServer(&base, "", 2099);
    handy_exitif_log(!svr, "start udp server failed");
    svr->onMsg([](const UdpServerPtr &p, Buffer buf, Ip4Addr peer) {
        handy_info_log("echo msg: %s to %s", buf.data(), peer.toString().c_str());
        p->sendTo(buf, peer);
    });
    base.loop();
}
