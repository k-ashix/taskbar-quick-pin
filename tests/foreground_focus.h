// Pure "how do we force a window to the foreground" decision.
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp, ForceForegroundWindow) and its unit tests
// (foreground_focus_test.cpp), with no Win32 deps.
//
// Bug this pins ("when another app -- e.g. the real windhawk.exe -- is open in
// the foreground, clicking a pinned dock icon does not open/focus it"):
//
//   14:10:47 RESOLVER: dock icon | ...Antigravity IDE.exe   <- resolve OK
//   ...but the app never comes to the front.
//
// SmartLaunch's already-running path calls SetForegroundWindow(running) from
// explorer.exe's worker thread. Windows' foreground lock (SPI_SETFOREGROUNDLOCK
// TIMEOUT) REFUSES a cross-thread SetForegroundWindow when a DIFFERENT thread
// currently owns the foreground -- the call silently fails and the taskbar
// button just flashes. This is exactly why "when windhawk.exe is foreground it
// skips the action": the foreground app holds the lock.
//
// Fix contract: before SetForegroundWindow, decide whether we must attach our
// thread's input queue to the CURRENT foreground thread's input queue
// (AttachThreadInput) so the OS treats the call as coming from the foreground
// thread. We must attach when:
//   * a foreground window exists, AND
//   * it is owned by a DIFFERENT thread than the target window, AND
//   * that foreground thread is also different from OUR (calling) thread.
// We must NOT attach a thread to itself (AttachThreadInput fails / is a no-op),
// and there is nothing to attach to when there is no foreground window or the
// target is already foreground.

#ifndef TASKBAR_QUICK_PIN_FOREGROUND_FOCUS_H
#define TASKBAR_QUICK_PIN_FOREGROUND_FOCUS_H

#include <cstdint>

// Thread ids as plain integers (GetWindowThreadProcessId / GetCurrentThreadId
// results). 0 means "none / no such window".
using TidT = std::uint32_t;

struct ForegroundFocusPlan {
    bool  needsAttach;  // must AttachThreadInput(callerTid, foregroundTid, TRUE)?
    TidT  attachToTid;  // the foreground thread to attach to (0 when none)
};

// Decide how to force `targetTid`'s window to the foreground, given:
//   callerTid     : the thread calling SetForegroundWindow (our worker thread)
//   targetTid     : thread owning the window we want in front (0 if unknown)
//   foregroundTid : thread owning the CURRENT foreground window (0 if none)
static inline ForegroundFocusPlan PlanForegroundFocus(TidT callerTid,
                                                      TidT targetTid,
                                                      TidT foregroundTid) {
    ForegroundFocusPlan plan{ false, 0 };

    // No foreground window at all -> SetForegroundWindow is allowed directly.
    if (foregroundTid == 0)
        return plan;

    // Target is already the foreground thread -> nothing to do.
    if (targetTid != 0 && foregroundTid == targetTid)
        return plan;

    // The foreground thread IS our own thread -> we already own foreground
    // input; a direct SetForegroundWindow is honored, and attaching a thread to
    // itself is invalid.
    if (foregroundTid == callerTid)
        return plan;

    // A different thread owns the foreground: attach to it so the OS treats our
    // SetForegroundWindow as coming from the foreground input queue.
    plan.needsAttach = true;
    plan.attachToTid = foregroundTid;
    return plan;
}

#endif  // TASKBAR_QUICK_PIN_FOREGROUND_FOCUS_H
