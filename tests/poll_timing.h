// Pure poll-loop timing math for the Explorer-workspace restore.
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp, WaitForNewExplorerAppForFolder / WaitForNewExplorerTab)
// and its unit tests (poll_timing_test.cpp), with no Win32 deps.
//
// Latency this pins ("clicking a pinned WORKSPACE/Explorer pin opens noticeably
// slower than an app pin"): the restore ShellExecute's the first Explorer window
// and then busy-waits for it to appear by polling EnumerateExplorerTabs() every
// POLL interval. With an 80ms interval, the window is typically ready within one
// interval, so up to ~80ms is pure dead time before the first Navigate -- and
// again per additional tab. A tighter interval cuts the average wait roughly in
// half per interval while keeping the SAME overall timeout budget.
//
// Fix contract:
//   * The poll interval is reduced (80ms -> ~20ms) but the total timeout budget
//     is UNCHANGED, so we never wait longer overall, only detect readiness sooner.
//   * Iterations stay BOUNDED: PollIterations(timeout, interval) is finite and
//     equals ceil(timeout/interval); a 0 interval is treated as invalid (clamped)
//     so the loop can never spin unbounded.
//   * Expected wasted wait for a resource that becomes ready at a uniformly random
//     time within one interval is interval/2, so a smaller interval strictly
//     reduces expected latency.

#ifndef TASKBAR_QUICK_PIN_POLL_TIMING_H
#define TASKBAR_QUICK_PIN_POLL_TIMING_H

// The tightened poll granularity used by the restore wait loops. Small enough to
// shave the perceptible first-window delay, large enough not to burn CPU on the
// COM/UIA enumeration each iteration.
static const int kRestorePollIntervalMs = 20;

// Clamp a poll interval into a safe, bounded range [1, timeout]. A non-positive
// interval would mean an unbounded/instant spin; cap at the timeout so there is
// always at least one wait.
static inline int ClampPollInterval(int intervalMs, int timeoutMs) {
    if (intervalMs < 1) intervalMs = 1;
    if (timeoutMs > 0 && intervalMs > timeoutMs) intervalMs = timeoutMs;
    return intervalMs;
}

// Number of poll iterations for a given timeout/interval = ceil(timeout/interval).
// Always finite and >= 0.
static inline int PollIterations(int timeoutMs, int intervalMs) {
    if (timeoutMs <= 0) return 0;
    intervalMs = ClampPollInterval(intervalMs, timeoutMs);
    return (timeoutMs + intervalMs - 1) / intervalMs;
}

// Expected wasted wait (x1000 to stay integer) when a resource becomes ready at
// a uniformly random point within one interval: interval/2. Used only to assert
// "smaller interval => smaller expected latency" in tests.
static inline int ExpectedWastedWaitMsX2(int intervalMs) {
    return intervalMs;  // (interval/2) * 2
}

#endif  // TASKBAR_QUICK_PIN_POLL_TIMING_H
