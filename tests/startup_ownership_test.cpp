// Unit tests for DecideStartupOwnership: the tri-state startup gate that stops
// a cold-boot taskbar race from permanently disabling the dock (roadmap Issue 1)
// while still preventing a second explorer.exe from drawing a duplicate dock.
//
// The bug: Wh_ModInit hard-failed (returned FALSE) whenever this process did not
// own Shell_TrayWnd. On a cold Explorer start the taskbar isn't up yet, so the
// owner PID is 0 and the ONE true shell process wrongly disowned the dock and
// never built it. The fix: DEFER on an unresolved owner and re-probe in the
// worker's boot poll, instead of failing init.

#include "startup_ownership.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- The bug: taskbar not up yet must DEFER, never disown -------------------

static void test_unresolved_taskbar_defers_not_fails() {
    // Cold boot / sign-in: Shell_TrayWnd not found yet -> owner PID 0. The
    // process must keep waiting, not permanently disable the dock.
    CHECK(DecideStartupOwnership(/*currentPid=*/5936UL, /*taskbarOwnerPid=*/0UL) == STARTUP_DEFER);
    CHECK(DecideStartupOwnership(/*currentPid=*/16032UL, /*taskbarOwnerPid=*/0UL) == STARTUP_DEFER);
}

// ---- The shell process owns the dock once the taskbar resolves --------------

static void test_taskbar_owner_owns_dock() {
    const unsigned long kShellPid = 5936UL;
    CHECK(DecideStartupOwnership(/*currentPid=*/kShellPid, /*taskbarOwnerPid=*/kShellPid) == STARTUP_OWN);
}

// ---- A second explorer.exe stays idle -- no duplicate dock ------------------

static void test_second_explorer_disowns() {
    const unsigned long kShellPid = 5936UL;
    CHECK(DecideStartupOwnership(/*currentPid=*/16032UL, /*taskbarOwnerPid=*/kShellPid) == STARTUP_DISOWN);
}

int main() {
    test_unresolved_taskbar_defers_not_fails();
    test_taskbar_owner_owns_dock();
    test_second_explorer_disowns();

    if (g_failures == 0) {
        std::printf("startup_ownership: ALL PASS\n");
        return 0;
    }
    std::printf("startup_ownership: %d FAILURE(S)\n", g_failures);
    return 1;
}
