// Unit tests for ProcessShouldOwnDock: the single-instance gate that stops a
// second explorer.exe process from drawing a duplicate dock and running a
// competing drag resolver.
//
// Bug (from the field log): two explorer.exe PIDs (5936 and 16032) each created
// an overlay (RepositionOverlay w=260 vs w=227) and each ran the resolver, which
// then disagreed (one "dock icon", one "RESOLVER MISS: all layers failed"),
// breaking every drag after a pinned Explorer window was opened.
//
// Contract: exactly the taskbar-owning process draws the dock. Any other
// explorer.exe -- including the transient one the workspace-restore path spawns
// -- must not.

#include "single_instance_gate.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- The bug: a second explorer.exe must NOT draw its own dock --------------

static void test_second_explorer_does_not_own_dock() {
    // Shell (taskbar) is PID 5936; the workspace-restore-spawned explorer is
    // 16032. Only the shell process may own the dock.
    const unsigned long kShellPid = 5936UL;
    CHECK(ProcessShouldOwnDock(/*currentPid=*/16032UL, /*taskbarOwnerPid=*/kShellPid) == false);
}

// ---- Still works: the taskbar-owning explorer.exe DOES own the dock ---------

static void test_shell_process_owns_dock() {
    const unsigned long kShellPid = 5936UL;
    CHECK(ProcessShouldOwnDock(/*currentPid=*/kShellPid, /*taskbarOwnerPid=*/kShellPid) == true);
}

// ---- Unknown owner: never claim ownership -----------------------------------

static void test_unresolved_taskbar_owner_never_owns() {
    // taskbarOwnerPid == 0 means Shell_TrayWnd could not be resolved. If two
    // processes both fail to resolve and both claimed ownership, we'd be back to
    // a duplicate dock. So an unknown owner never owns.
    CHECK(ProcessShouldOwnDock(/*currentPid=*/5936UL, /*taskbarOwnerPid=*/0UL) == false);
    CHECK(ProcessShouldOwnDock(/*currentPid=*/16032UL, /*taskbarOwnerPid=*/0UL) == false);
}

// ---- A process is never its own accidental owner via pid 0 ------------------

static void test_current_pid_zero_does_not_match_zero_owner() {
    // Defensive: a bogus current pid of 0 must not match an unresolved owner.
    CHECK(ProcessShouldOwnDock(/*currentPid=*/0UL, /*taskbarOwnerPid=*/0UL) == false);
}

int main() {
    test_second_explorer_does_not_own_dock();
    test_shell_process_owns_dock();
    test_unresolved_taskbar_owner_never_owns();
    test_current_pid_zero_does_not_match_zero_owner();

    if (g_failures == 0) {
        std::printf("single_instance_gate: ALL PASS\n");
        return 0;
    }
    std::printf("single_instance_gate: %d FAILURE(S)\n", g_failures);
    return 1;
}
