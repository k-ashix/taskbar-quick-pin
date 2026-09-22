// Unit tests for PlanForegroundFocus: forcing a pinned app to the foreground
// must work even when a DIFFERENT app (e.g. the real windhawk.exe) currently
// holds the foreground -- which is when clicks were being "skipped".

#include "foreground_focus.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- THE BUG: a different foreground app blocks the plain call --------------

static void test_foreground_windhawk_requires_attach() {
    // caller = explorer worker (10), target = Antigravity IDE (20),
    // foreground = windhawk.exe (30). Must attach to the windhawk thread.
    ForegroundFocusPlan p = PlanForegroundFocus(/*caller=*/10, /*target=*/20,
                                                /*foreground=*/30);
    CHECK(p.needsAttach);
    CHECK(p.attachToTid == 30);
}

// ---- No attach needed in the benign cases -----------------------------------

static void test_no_foreground_window_no_attach() {
    ForegroundFocusPlan p = PlanForegroundFocus(10, 20, /*foreground=*/0);
    CHECK(!p.needsAttach);
    CHECK(p.attachToTid == 0);
}

static void test_target_already_foreground_no_attach() {
    ForegroundFocusPlan p = PlanForegroundFocus(10, /*target=*/20,
                                                /*foreground=*/20);
    CHECK(!p.needsAttach);
}

static void test_caller_owns_foreground_no_self_attach() {
    // Our own thread is the foreground thread: a direct call works and we must
    // never AttachThreadInput a thread to itself.
    ForegroundFocusPlan p = PlanForegroundFocus(/*caller=*/10, /*target=*/20,
                                                /*foreground=*/10);
    CHECK(!p.needsAttach);
    CHECK(p.attachToTid != 10);
}

static void test_unknown_target_still_attaches_to_foreground() {
    // Even if we could not resolve the target thread (0), a foreign foreground
    // still requires the attach dance to win the foreground.
    ForegroundFocusPlan p = PlanForegroundFocus(/*caller=*/10, /*target=*/0,
                                                /*foreground=*/30);
    CHECK(p.needsAttach);
    CHECK(p.attachToTid == 30);
}

int main() {
    test_foreground_windhawk_requires_attach();
    test_no_foreground_window_no_attach();
    test_target_already_foreground_no_attach();
    test_caller_owns_foreground_no_self_attach();
    test_unknown_target_still_attaches_to_foreground();

    if (g_failures == 0) {
        std::printf("foreground_focus: ALL PASS\n");
        return 0;
    }
    std::printf("foreground_focus: %d FAILURE(S)\n", g_failures);
    return 1;
}
