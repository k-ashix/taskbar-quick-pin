// Pure lifecycle logic for LaunchWorkspaceAsync's worker thread(s).
//
// Shared between the Windhawk mod (taskbar-quick-pin.wh.cpp) and its unit tests
// (launch_thread_lifecycle_test.cpp) so the two race-critical decisions are
// verified with no Win32 deps.
//
// Bugs this pins (review finding #4):
//
//   Bug A -- dropped join point / use-after-unload crash.
//     The old code kept a SINGLE handle slot (g_launchWorkspaceThread). When a
//     second launch started while a previous launch thread was STILL RUNNING,
//     it did `CloseHandle(old); slot = new;` -- closing the running thread's
//     handle and discarding it. Wh_ModUninit then had nothing to wait on for
//     that thread, so it kept executing mod code (COM / ShellExecuteExW / Sleep
//     loops) after Windhawk FreeLibrary'd the image -> its return address lives
//     in the unmapped image -> the host crashes.
//
//     Contract: a still-running handle is NEVER dropped. Track ALL outstanding
//     handles; only reap (close + remove) the ones that have finished. On
//     teardown, EVERY outstanding handle is joined.
//
//   Bug B -- unload stall.
//     RestoreExplorerWindowGroup's WaitForNewExplorerAppForFolder / ...Tab loops
//     did a blind `Sleep(80)` up to timeoutMs (4500 / 2500) per tab and only
//     checked the stop flag once, at thread entry. Disabling the mod mid-restore
//     made Wh_ModUninit's WaitForSingleObject(..., INFINITE) block for ~15 s.
//
//     Contract: each wait iteration must abort the moment teardown is signaled,
//     regardless of how much timeout remains.

#ifndef TASKBAR_QUICK_PIN_LAUNCH_THREAD_LIFECYCLE_H
#define TASKBAR_QUICK_PIN_LAUNCH_THREAD_LIFECYCLE_H

#include <vector>
#include <cstddef>

// ---- Bug B: wait-loop abort decision ---------------------------------------
//
// A polling wait iteration should CONTINUE only while there is time left AND
// teardown has not been signaled. `exitSignaled` models
// WaitForSingleObject(g_exitEvent, ...) == WAIT_OBJECT_0.
//
// The bug was that exitSignaled was never consulted inside the loop. The fixed
// predicate makes the loop stop instantly on teardown.
static inline bool ShouldContinueWait(bool timeRemaining, bool exitSignaled) {
    return timeRemaining && !exitSignaled;
}

// ---- Bug A: outstanding launch-handle tracking -----------------------------
//
// Models the handle set without any real HANDLEs. `finished == false` means the
// thread is still running (its handle must not be dropped).
struct LaunchHandle {
    int  id;         // stand-in for a HANDLE value
    bool finished;   // WaitForSingleObject(h, 0) == WAIT_OBJECT_0
    bool joined;     // set when the tracker "closes"/reaps it
};

class LaunchHandleTracker {
public:
    // Reap (join+close) every handle that has already finished, then start
    // tracking the new launch. Running handles are LEFT in the set so teardown
    // can still join them -- this is the whole point of Bug A.
    void RegisterLaunch(int newId) {
        ReapFinished();
        handles_.push_back(LaunchHandle{newId, /*finished=*/false, /*joined=*/false});
    }

    // Mark a tracked launch as finished (thread returned).
    void MarkFinished(int id) {
        for (auto& h : handles_) if (h.id == id) h.finished = true;
    }

    // Called from Wh_ModUninit: join (and close) EVERY outstanding handle.
    void JoinAll() {
        for (auto& h : handles_) h.joined = true;
        // In the real code the handles are also closed + the vector cleared.
        // We keep them so tests can assert every one was joined.
    }

    std::size_t OutstandingCount() const {
        std::size_t n = 0;
        for (const auto& h : handles_) if (!h.joined) ++n;
        return n;
    }

    bool AllJoined() const {
        for (const auto& h : handles_) if (!h.joined) return false;
        return true;
    }

    bool IsTracked(int id) const {
        for (const auto& h : handles_) if (h.id == id && !h.joined) return true;
        return false;
    }

private:
    void ReapFinished() {
        std::vector<LaunchHandle> keep;
        for (const auto& h : handles_) {
            if (h.finished) continue;   // finished -> reaped/removed
            keep.push_back(h);          // still running -> KEEP (never drop)
        }
        handles_.swap(keep);
    }

    std::vector<LaunchHandle> handles_;
};

#endif  // TASKBAR_QUICK_PIN_LAUNCH_THREAD_LIFECYCLE_H
