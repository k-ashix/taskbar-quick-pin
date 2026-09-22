// Unit tests for IsPressReleaseAClick: a stationary press-release must always
// count as a click, even when the polling loop noticed the release "late".
//
// Bug (from the log): a normal click on a dock icon (workspace OR app, e.g.
// Edge) resolved correctly but never launched, because the old wasClick gate
// also required elapsed < 300ms and the polling loop routinely blew past that
// after a launch/focus change. Clicks "got consumed instead of action".

#include "click_vs_drag.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- The bug: a stationary release is a click regardless of timing ----------

static void test_stationary_release_is_a_click() {
    // No movement. Old code also demanded elapsed < 300ms; the poll often saw
    // the release later than that, dropping the click. Timing must not matter.
    CHECK(IsPressReleaseAClick(0, 0) == true);
}

static void test_tiny_jitter_still_a_click() {
    // A couple pixels of hand jitter within the click box is still a click.
    CHECK(IsPressReleaseAClick(2, 3) == true);
    CHECK(IsPressReleaseAClick(CLICK_MAX_MOVE_PX, CLICK_MAX_MOVE_PX) == true);
}

// ---- Still correct: real movement is NOT a click ----------------------------
// (In the mod, movement > DRAG_THRESHOLD_PX would already have advanced the
// state to DRAG_DRAGGING, so this branch would not even run; the movement guard
// here is the belt-and-braces backstop.)

static void test_movement_beyond_box_is_not_a_click() {
    CHECK(IsPressReleaseAClick(CLICK_MAX_MOVE_PX + 1, 0) == false);
    CHECK(IsPressReleaseAClick(0, CLICK_MAX_MOVE_PX + 1) == false);
    CHECK(IsPressReleaseAClick(40, 40) == false);
}

int main() {
    test_stationary_release_is_a_click();
    test_tiny_jitter_still_a_click();
    test_movement_beyond_box_is_not_a_click();

    if (g_failures == 0) {
        std::printf("click_vs_drag: ALL PASS\n");
        return 0;
    }
    std::printf("click_vs_drag: %d FAILURE(S)\n", g_failures);
    return 1;
}
