#include <handy/handy.h>

using namespace std;
using namespace handy;

int main(int argc, const char *argv[]) {
    handy_setloglevel("TRACE");
    EventBase base;
    Signal::signal(SIGINT, [&] { base.exit(); });
    TcpConnPtr con = TcpConn::createConnection(&base, "127.0.0.1", 2099, 3000);
    con->setReconnectInterval(3000);
    con->onMsg(new LengthCodec, [](const TcpConnPtr &con, Slice msg) { handy_info_log("recv msg: %.*s", (int) msg.size(), msg.data()); });
    con->onState([=](const TcpConnPtr &con) {
        handy_info_log("onState called state: %d", con->getState());
        if (con->getState() == TcpConn::Connected) {
            con->sendMsg("hello");
        }
    });
    base.loop();
    handy_info_log("program exited");
}