// Unit tests for ExceedsDragThreshold (Stage 2 - Real Drag Detection).
//
// Guards the refactor from a fixed 6px DRAG_THRESHOLD_PX to per-axis
// GetSystemMetrics(SM_CXDRAG/SM_CYDRAG). The threshold must be per-axis, a
// strict "exceeds" test, and satisfied by either axis alone.

#include "drag_threshold.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Typical Windows defaults are SM_CXDRAG == SM_CYDRAG == 4, but the mod reads
// them at runtime, so tests assert the RELATIONSHIP, not a magic number.

static void test_no_movement_is_not_a_drag() {
    CHECK(ExceedsDragThreshold(0, 0, 4, 4) == false);
}

static void test_exactly_at_threshold_is_still_a_press() {
    // Strict ">": landing exactly on the threshold has NOT crossed it yet.
    CHECK(ExceedsDragThreshold(4, 0, 4, 4) == false);
    CHECK(ExceedsDragThreshold(0, 4, 4, 4) == false);
    CHECK(ExceedsDragThreshold(4, 4, 4, 4) == false);
}

static void test_one_past_threshold_on_x_is_a_drag() {
    CHECK(ExceedsDragThreshold(5, 0, 4, 4) == true);
}

static void test_one_past_threshold_on_y_is_a_drag() {
    CHECK(ExceedsDragThreshold(0, 5, 4, 4) == true);
}

static void test_threshold_is_per_axis_not_shared() {
    // A tall, narrow drag box: 2px horizontal tolerance, 20px vertical. Moving
    // 10px sideways must trip the X axis even though it's well under the Y one.
    // A single shared threshold (e.g. max of the two) would wrongly miss this.
    CHECK(ExceedsDragThreshold(10, 0, 2, 20) == true);   // crosses narrow X
    CHECK(ExceedsDragThreshold(0, 10, 2, 20) == false);  // within tall Y
}

static void test_respects_larger_configured_threshold() {
    // If the user configured a big drag box, small moves stay a press.
    CHECK(ExceedsDragThreshold(6, 6, 12, 12) == false);
    CHECK(ExceedsDragThreshold(13, 0, 12, 12) == true);
}

int main() {
    test_no_movement_is_not_a_drag();
    test_exactly_at_threshold_is_still_a_press();
    test_one_past_threshold_on_x_is_a_drag();
    test_one_past_threshold_on_y_is_a_drag();
    test_threshold_is_per_axis_not_shared();
    test_respects_larger_configured_threshold();

    if (g_failures == 0) {
        std::printf("drag_threshold: ALL PASS\n");
        return 0;
    }
    std::printf("drag_threshold: %d FAILURE(S)\n", g_failures);
    return 1;
}
