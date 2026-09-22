// Pure startup-ownership decision for the Quick Pin dock.
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp) and its unit tests (startup_ownership_test.cpp),
// so the "should this explorer.exe activate the dock, stay idle, or keep
// waiting for the taskbar" decision is verified with no Win32 deps.
//
// Background (roadmap Issue 1):
//   Wh_ModInit used to call ProcessOwnsTaskbar() and return FALSE when this
//   process did not own Shell_TrayWnd. But on a COLD Explorer start / sign-in
//   the shell's Shell_TrayWnd may not exist yet when the mod initialises, so
//   the owner PID resolves to 0. The old code treated that as "not the owner"
//   and hard-failed init -- the one true shell process then never built the
//   dock (it depended on Windhawk retrying init, which is not guaranteed on
//   every host/timing). Ownership must NOT be a hard init failure.
//
// The fix keeps init succeeding and moves the ownership decision into the
// worker thread's STATE_BOOT poll, where the taskbar is re-probed until it
// appears. This pure helper models that tri-state decision:
//
//   OWN   -> taskbar resolved and it belongs to THIS process: activate the dock.
//   DISOWN-> taskbar resolved and it belongs to ANOTHER process: stay idle
//            forever (a second explorer.exe must never draw a duplicate dock).
//   DEFER -> taskbar not resolved yet (owner PID 0): keep polling during boot;
//            do NOT activate and do NOT permanently disown.

#ifndef TASKBAR_QUICK_PIN_STARTUP_OWNERSHIP_H
#define TASKBAR_QUICK_PIN_STARTUP_OWNERSHIP_H

enum StartupOwnership {
    STARTUP_DEFER = 0,   // taskbar not up yet -- keep polling during boot
    STARTUP_OWN,         // this process owns Shell_TrayWnd -- activate the dock
    STARTUP_DISOWN       // another process owns it -- stay idle (no dupe dock)
};

// currentPid       : GetCurrentProcessId() of this explorer.exe.
// taskbarOwnerPid  : PID owning Shell_TrayWnd, or 0 if it could not be resolved.
//
// Contract:
//   * owner PID 0 (Shell_TrayWnd not found yet) -> DEFER. Never a hard failure,
//     never ownership. This is the cold-boot / sign-in case.
//   * owner PID == current PID -> OWN.
//   * owner PID resolved but != current PID -> DISOWN (another explorer.exe).
static inline StartupOwnership DecideStartupOwnership(unsigned long currentPid,
                                                      unsigned long taskbarOwnerPid) {
    if (taskbarOwnerPid == 0UL) return STARTUP_DEFER;
    return (taskbarOwnerPid == currentPid) ? STARTUP_OWN : STARTUP_DISOWN;
}

#endif  // TASKBAR_QUICK_PIN_STARTUP_OWNERSHIP_H
