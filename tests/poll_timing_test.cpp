// Unit tests for the workspace-restore poll timing: a tighter interval must cut
// expected latency without changing the total timeout budget or spinning
// unbounded.

#include "poll_timing.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- THE FIX: tighter interval -> lower expected latency, same budget -------

static void test_tighter_interval_reduces_expected_wait() {
    // The old 80ms poll wasted ~40ms on average; the new 20ms wastes ~10ms.
    CHECK(ExpectedWastedWaitMsX2(kRestorePollIntervalMs) <
          ExpectedWastedWaitMsX2(80));
}

static void test_new_interval_is_tighter_than_old() {
    CHECK(kRestorePollIntervalMs < 80);
    CHECK(kRestorePollIntervalMs >= 1);
}

static void test_total_timeout_budget_unchanged() {
    // Same 4500ms first-window budget: more iterations, same wall-clock cap.
    int iters20 = PollIterations(4500, kRestorePollIntervalMs);
    int iters80 = PollIterations(4500, 80);
    CHECK(iters20 > iters80);                 // finer granularity
    CHECK(iters20 * kRestorePollIntervalMs >= 4500);   // still covers full budget
    CHECK((iters20 - 1) * kRestorePollIntervalMs < 4500); // but no gross overshoot
}

// ---- Bounded / safe iteration counts ----------------------------------------

static void test_iterations_are_bounded_and_finite() {
    CHECK(PollIterations(4500, 20) == 225);
    CHECK(PollIterations(2500, 20) == 125);
    CHECK(PollIterations(0, 20) == 0);        // no timeout -> no polling
}

static void test_zero_interval_is_clamped_not_unbounded() {
    // A 0 or negative interval must never yield an infinite loop.
    CHECK(ClampPollInterval(0, 4500) >= 1);
    CHECK(ClampPollInterval(-100, 4500) >= 1);
    int iters = PollIterations(4500, 0);
    CHECK(iters > 0 && iters <= 4500);        // finite, at most 1ms-granularity
}

static void test_interval_never_exceeds_timeout() {
    // With a tiny timeout, one wait of the whole timeout is enough.
    CHECK(ClampPollInterval(20, 5) == 5);
    CHECK(PollIterations(5, 20) == 1);
}

int main() {
    test_tighter_interval_reduces_expected_wait();
    test_new_interval_is_tighter_than_old();
    test_total_timeout_budget_unchanged();
    test_iterations_are_bounded_and_finite();
    test_zero_interval_is_clamped_not_unbounded();
    test_interval_never_exceeds_timeout();

    if (g_failures == 0) {
        std::printf("poll_timing: ALL PASS\n");
        return 0;
    }
    std::printf("poll_timing: %d FAILURE(S)\n", g_failures);
    return 1;
}
