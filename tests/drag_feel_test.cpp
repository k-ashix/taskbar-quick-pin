// Tests for the two pure decisions behind the "drag to break feels mechanical
// and sometimes miss-hits" report:
//
//   1. DragWorkerPollMs -- how long the worker sleeps between frames for a given
//      drag state. The miss-hit bug: while the mouse is DOWN but the move
//      threshold has not yet been crossed (DRAG_PRESS), the loop fell back to
//      the 16ms / 50ms idle cadence, so a quick flick to tear an icon off was
//      sampled too coarsely and read as a click instead of a drag. PRESS must
//      poll at the same fast cadence as an active drag.
//
//   2. RopeShouldBreak -- the finite-rope tear trigger. A hard `dist >= max`
//      binary cut fires the instant a single (possibly noisy) cursor sample
//      touches the limit, which reads as an abrupt mechanical snap. A small
//      over-stretch margin gives the thread a touch of give before it tears.

#include "drag_feel.h"
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                            \
    do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

// --- 1. worker cadence ------------------------------------------------------
static void test_press_polls_fast() {
    // The regression: PRESS used to fall through to 16/50ms. It must be fast.
    CHECK(DragWorkerPollMs(DS_PRESS, /*animating*/false, /*idleFrames*/50, /*notStable*/false)
          == DRAG_FAST_POLL_MS);
}

static void test_active_drag_and_reorder_poll_fast() {
    CHECK(DragWorkerPollMs(DS_DRAGGING, false, 0, false) == DRAG_FAST_POLL_MS);
    CHECK(DragWorkerPollMs(DS_REORDER,  false, 0, false) == DRAG_FAST_POLL_MS);
}

static void test_animation_polls_fast() {
    CHECK(DragWorkerPollMs(DS_IDLE, /*animating*/true, 999, false) == DRAG_FAST_POLL_MS);
}

static void test_idle_polls_medium_then_slow() {
    CHECK(DragWorkerPollMs(DS_IDLE, false, /*idleFrames*/3,  false) == DRAG_MED_POLL_MS);
    CHECK(DragWorkerPollMs(DS_IDLE, false, /*idleFrames*/50, false) == DRAG_SLOW_POLL_MS);
}

static void test_boot_never_slow() {
    // During boot (not stable) the loop must not drop to the slow cadence.
    CHECK(DragWorkerPollMs(DS_IDLE, false, /*idleFrames*/50, /*notStable*/true) == DRAG_MED_POLL_MS);
}

// --- 2. break trigger with give --------------------------------------------
static void test_no_break_exactly_at_limit() {
    // Right AT the limit should not tear yet -- the small margin gives the
    // thread a beat of stretch so a boundary-straddling sample doesn't cut.
    CHECK(RopeShouldBreak(/*dist*/450.f, /*maxStretch*/450.f,
                          /*alreadyBreaking*/false, /*dropZoneActive*/false) == false);
}

static void test_break_past_margin() {
    // Pulled clearly past the limit -> tear.
    CHECK(RopeShouldBreak(500.f, 450.f, false, false) == true);
}

static void test_no_break_while_already_breaking() {
    CHECK(RopeShouldBreak(999.f, 450.f, /*alreadyBreaking*/true, false) == false);
}

static void test_no_break_in_dock_zone() {
    // Sliding along the taskbar near the dock must never auto-unpin.
    CHECK(RopeShouldBreak(999.f, 450.f, false, /*dropZoneActive*/true) == false);
}

static void test_margin_is_small() {
    // The give should be a light polish, not a second break length.
    CHECK(ROPE_BREAK_MARGIN_PX > 0.f && ROPE_BREAK_MARGIN_PX <= 40.f);
}

int main() {
    test_press_polls_fast();
    test_active_drag_and_reorder_poll_fast();
    test_animation_polls_fast();
    test_idle_polls_medium_then_slow();
    test_boot_never_slow();
    test_no_break_exactly_at_limit();
    test_break_past_margin();
    test_no_break_while_already_breaking();
    test_no_break_in_dock_zone();
    test_margin_is_small();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
