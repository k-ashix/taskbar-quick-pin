// Pure decision logic for the worker loop's unsupported-layout branch.
//
// Mirrored (like the other tests/*.h) from taskbar-quick-pin.wh.cpp WorkerThread
// so it can be unit-tested with no Win32 deps.
//
// Bug this pins (Issue 2 -- "unsupported-layout path is a busy loop, 100% of a
// core in explorer.exe"): the worker loop head is
//     while (WaitForSingleObject(g_exitEvent, 0) != WAIT_OBJECT_0)   // 0ms poll
// and EVERY adaptive sleep (WaitForSingleObject(g_exitEvent, 8/16/50)) lives
// BELOW the drag block. The early
//     if (g_layoutUnsupported) { lastLDown = lDown; continue; }
// jumped straight back to that zero-timeout loop head, so on an unsupported
// layout (every Windows 10 machine, left-aligned Windows 11) the worker spun
// with no wait at all -- burning a full core -- and never released a
// timeBeginPeriod(1) that was held when the layout flipped.
//
// Fix contract, modelled here:
//   * On the unsupported-layout branch the worker MUST wait on g_exitEvent for a
//     positive, bounded interval before looping (matching every other early
//     continue), so CPU per second is O(1000/interval) iterations, not unbounded.
//   * It MUST release the high-res timer (SetHighResTimer(false)) on that branch
//     so a timeBeginPeriod(1) isn't held while idle.
//   * A signalled exit event MUST break the idle wait promptly (disable/reload
//     tears down instantly rather than blocking).

#ifndef TASKBAR_QUICK_PIN_UNSUPPORTED_LAYOUT_IDLE_H
#define TASKBAR_QUICK_PIN_UNSUPPORTED_LAYOUT_IDLE_H

// The wait applied on the unsupported-layout branch, mirroring
// WaitForSingleObject(g_exitEvent, 50) in the mod.
static const int kUnsupportedIdleWaitMs = 50;

// What the unsupported-layout branch does for one loop turn. Kept as a plain
// struct so a test can assert every observable effect of the branch.
struct UnsupportedBranchAction {
    bool releasesHighResTimer;  // must call SetHighResTimer(false)
    int  exitWaitMs;            // ms it parks on g_exitEvent before continuing
    bool continues;             // loops again (never falls through to drag code)
};

// Model of the fixed branch: release the timer, park on the exit event for a
// bounded interval, then continue. (The pre-fix bug was exitWaitMs == 0 and
// releasesHighResTimer == false.)
static inline UnsupportedBranchAction UnsupportedLayoutBranch() {
    UnsupportedBranchAction a;
    a.releasesHighResTimer = true;
    a.exitWaitMs           = kUnsupportedIdleWaitMs;
    a.continues            = true;
    return a;
}

// Upper bound on worker iterations over a wall-clock window when parked on the
// unsupported-layout branch. A positive per-turn wait makes this finite; a 0ms
// wait (the bug) makes it effectively unbounded.
static inline long IdleIterationsUpperBound(long windowMs, int perTurnWaitMs) {
    if (perTurnWaitMs < 1) return -1;   // unbounded / spin -- the bug
    return windowMs / perTurnWaitMs;
}

// Whether a wait of waitMs on the exit event returns due to the event being
// signalled (exit requested) vs. timing out. When signalled, the wait returns
// promptly regardless of waitMs, so teardown is not blocked.
static inline bool ExitWaitBreaksOnSignal(bool exitSignalled) {
    return exitSignalled;   // WAIT_OBJECT_0 the moment the event is set
}

#endif  // TASKBAR_QUICK_PIN_UNSUPPORTED_LAYOUT_IDLE_H
