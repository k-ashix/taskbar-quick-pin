// Unit tests for Issue 2: the unsupported-layout worker branch must idle on the
// exit event (bounded, low CPU) and release the high-res timer, instead of
// busy-looping at 100% of a core.

#include "unsupported_layout_idle.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- THE FIX: branch parks on the exit event, not a bare continue -----------

static void test_branch_waits_on_exit_event() {
    UnsupportedBranchAction a = UnsupportedLayoutBranch();
    // The pre-fix branch had exitWaitMs == 0 (bare `continue` -> 0ms loop head
    // poll). It must now wait a positive, bounded interval.
    CHECK(a.exitWaitMs > 0);
    CHECK(a.exitWaitMs == kUnsupportedIdleWaitMs);
    CHECK(a.continues);
}

static void test_branch_releases_high_res_timer() {
    UnsupportedBranchAction a = UnsupportedLayoutBranch();
    // A timeBeginPeriod(1) held when the layout flipped must be released, since
    // the drag code below (which used to call SetHighResTimer(false)) is skipped.
    CHECK(a.releasesHighResTimer);
}

// ---- CPU is bounded, not a spin ---------------------------------------------

static void test_iterations_are_bounded_over_a_second() {
    // With the 50ms park, one second of unsupported-layout idle is ~20 turns,
    // not an unbounded spin.
    long iters = IdleIterationsUpperBound(1000, kUnsupportedIdleWaitMs);
    CHECK(iters == 20);
    CHECK(iters > 0);
}

static void test_zero_wait_is_the_bug_and_is_unbounded() {
    // Model the pre-fix behaviour to prove the test would have caught it: a 0ms
    // wait yields an unbounded (sentinel -1) iteration count -- the busy loop.
    CHECK(IdleIterationsUpperBound(1000, 0) == -1);
}

static void test_smaller_wait_never_below_one_ms() {
    // Any positive wait keeps iterations finite; the fixed value is well above 1.
    CHECK(IdleIterationsUpperBound(1000, 1) == 1000);
    CHECK(kUnsupportedIdleWaitMs >= 1);
}

// ---- Teardown is still prompt ------------------------------------------------

static void test_exit_signal_breaks_idle_immediately() {
    // A disable/reload during the idle wait must return at once (WAIT_OBJECT_0),
    // not block for the full interval.
    CHECK(ExitWaitBreaksOnSignal(true));
    CHECK(!ExitWaitBreaksOnSignal(false));   // no signal -> times out, loops
}

int main() {
    test_branch_waits_on_exit_event();
    test_branch_releases_high_res_timer();
    test_iterations_are_bounded_over_a_second();
    test_zero_wait_is_the_bug_and_is_unbounded();
    test_smaller_wait_never_below_one_ms();
    test_exit_signal_breaks_idle_immediately();

    if (g_failures == 0) {
        std::printf("unsupported_layout_idle: ALL PASS\n");
        return 0;
    }
    std::printf("unsupported_layout_idle: %d FAILURE(S)\n", g_failures);
    return 1;
}
