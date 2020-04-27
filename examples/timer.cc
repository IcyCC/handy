#include <handy/handy.h>
using namespace handy;

int main(int argc, const char *argv[]) {
    EventBase base;
    Signal::signal(SIGINT, [&] { base.exit(); });
    handy_info_log("program begin");
    base.runAfter(200, []() { handy_info_log("a task in runAfter 200ms"); });
    base.runAfter(100, []() { handy_info_log("a task in runAfter 100ms interval 1000ms"); }, 1000);
    TimerId id = base.runAt(time(NULL) * 1000 + 300, []() { handy_info_log("a task in runAt now+300 interval 500ms"); }, 500);
    base.runAfter(2000, [&]() {
        handy_info_log("cancel task of interval 500ms");
        base.cancel(id);
    });
    base.runAfter(3000, [&]() { base.exit(); });
    base.loop();
}