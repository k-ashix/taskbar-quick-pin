// Unit tests for the LaunchWorkspaceAsync thread lifecycle (review finding #4).
//
// Bug A: a still-running launch thread's handle must never be dropped -- if a
//        second launch starts while the first is still running, BOTH must stay
//        tracked so Wh_ModUninit can join them before the image unloads.
// Bug B: the Explorer-restore wait loops must abort the instant teardown is
//        signaled, not keep Sleep(80)-polling for the full 2.5-4.5 s timeout.

#include "launch_thread_lifecycle.h"
#include <cstdio>

// ---- RED reference: the OLD single-slot model (review finding #4, Bug A) ----
// Reproduces the pre-fix logic so the test proves the bug is real. The single
// slot only reaps when the tracked thread has ALREADY finished; otherwise it
// closes+overwrites the running handle, dropping the join point. Kept here as a
// regression anchor -- OldSingleSlot must FAIL the "keep both" invariant that
// LaunchHandleTracker satisfies.
struct OldSingleSlot {
    int  id = 0;         // 0 == empty slot
    bool finished = false;
    bool joined = false;
    int  droppedRunning = 0;   // running handles silently discarded (the bug)

    void RegisterLaunch(int newId) {
        if (id != 0) {
            if (finished) {
                joined = true;            // reaped cleanly
            } else {
                ++droppedRunning;         // BUG: running handle dropped, never joined
            }
        }
        id = newId; finished = false; joined = false;
    }
};

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- Bug A: prove the OLD model was broken (regression anchor) --------------

static void test_old_single_slot_drops_a_running_handle() {
    // Documents the pre-fix defect: a second launch while the first is still
    // running discards the first's handle, so it can never be joined. This is
    // the crash the fix removes; the fixed tracker (below) must NOT do this.
    OldSingleSlot old;
    old.RegisterLaunch(1);          // running
    old.RegisterLaunch(2);          // running -> #1's handle dropped
    CHECK(old.droppedRunning == 1); // the bug, reproduced
}

// ---- Bug A ------------------------------------------------------------------

static void test_second_launch_while_first_running_keeps_both() {
    LaunchHandleTracker t;
    t.RegisterLaunch(1);            // first launch, still running
    t.RegisterLaunch(2);            // second launch starts BEFORE #1 finishes
    // The old single-slot code dropped #1 here. Both must remain tracked.
    CHECK(t.OutstandingCount() == 2);
    CHECK(t.IsTracked(1));
    CHECK(t.IsTracked(2));
}

static void test_finished_handle_is_reaped_on_next_launch() {
    LaunchHandleTracker t;
    t.RegisterLaunch(1);
    t.MarkFinished(1);              // thread #1 returned
    t.RegisterLaunch(2);            // reaps #1, tracks #2
    CHECK(t.OutstandingCount() == 1);
    CHECK(!t.IsTracked(1));
    CHECK(t.IsTracked(2));
}

static void test_teardown_joins_every_outstanding_handle() {
    LaunchHandleTracker t;
    t.RegisterLaunch(1);            // still running
    t.RegisterLaunch(2);            // still running
    t.JoinAll();                    // Wh_ModUninit
    CHECK(t.AllJoined());
    CHECK(t.OutstandingCount() == 0);
}

// ---- Bug B ------------------------------------------------------------------

static void test_wait_continues_only_while_time_left_and_not_torn_down() {
    // Normal case: time remaining, no teardown -> keep waiting.
    CHECK(ShouldContinueWait(/*timeRemaining=*/true, /*exitSignaled=*/false));
    // Timeout reached -> stop.
    CHECK(!ShouldContinueWait(false, false));
}

static void test_wait_aborts_immediately_on_teardown() {
    // Even with the full timeout still remaining, a teardown signal stops the
    // loop this iteration -- this is the ~15 s unload stall fix.
    CHECK(!ShouldContinueWait(/*timeRemaining=*/true, /*exitSignaled=*/true));
}

int main() {
    test_old_single_slot_drops_a_running_handle();
    test_second_launch_while_first_running_keeps_both();
    test_finished_handle_is_reaped_on_next_launch();
    test_teardown_joins_every_outstanding_handle();
    test_wait_continues_only_while_time_left_and_not_torn_down();
    test_wait_aborts_immediately_on_teardown();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
