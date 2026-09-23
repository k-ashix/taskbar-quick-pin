// Unit tests for the drag-to-pin feedback glow decision (edge_feedback_line.h).
//
// The dock's old right-edge feedback LINE was removed; drag feedback is now a
// lock-glow bloom. DragGlowFor is the pure decision for WHICH glow to show while
// an item is dragged:
//   * only for a PIN drag (a NEW app dragged IN) -- never an unpin / reorder /
//     rope-break drag of an already-pinned icon,
//   * only while the cursor is INSIDE the dock drop zone (never eager: nothing
//     before it enters),
//   * RED the moment it enters a FULL dock (pin limit), decided up front so it is
//     never a green-then-red flash; GREEN while there is still room.
// The mod samples this every drag frame, shows it sustained while the icon is
// held, and clears it the instant this returns DRAGGLOW_NONE (left the zone, or
// released / dropped). Self-contained (own main()); compiled by run_tests.ps1.

#include "edge_feedback_line.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_pin_drag_in_zone_with_room_shows_green() {
    // A NEW app dragged into the dock, cursor in the drop zone, room to spare.
    CHECK(DragGlowFor(/*pinningDrag=*/true, /*inDropZone=*/true, /*dockFull=*/false) == DRAGGLOW_GREEN);
}

static void test_full_dock_shows_red_on_entry_not_green_first() {
    // R3: when the pin limit is already full, entering the zone shows RED right
    // away -- never a green flash that then turns red.
    CHECK(DragGlowFor(true, true, /*dockFull=*/true) == DRAGGLOW_RED);
}

static void test_not_eager_nothing_before_entering_zone() {
    // R3: detection is not eager -- nothing shows until the icon is actually IN
    // the zone, whether or not the dock is full.
    CHECK(DragGlowFor(true, /*inDropZone=*/false, false) == DRAGGLOW_NONE);
    CHECK(DragGlowFor(true, /*inDropZone=*/false, true)  == DRAGGLOW_NONE);
}

static void test_unpin_reorder_or_rope_drag_never_glows() {
    // R1: dragging an ALREADY-pinned icon (off to unpin / reorder / break rope)
    // is NOT a pin drag, so it never fires the feedback glow -- even in the zone,
    // and regardless of whether the dock is full.
    CHECK(DragGlowFor(/*pinningDrag=*/false, true,  false) == DRAGGLOW_NONE);
    CHECK(DragGlowFor(/*pinningDrag=*/false, true,  true)  == DRAGGLOW_NONE);
    CHECK(DragGlowFor(/*pinningDrag=*/false, false, false) == DRAGGLOW_NONE);
}

static void test_held_in_zone_is_sustained_then_clears_on_leave() {
    // R2 contract the mod relies on: while the icon is HELD in the zone the
    // decision keeps returning the SAME non-NONE glow (sustained, not a one-shot
    // flash), and flips to NONE the instant it leaves -- which is the mod's cue
    // to tear the glow down (also what happens on release / drop, when the drag
    // ends and there is no longer an in-zone pin drag).
    CHECK(DragGlowFor(true, true, false) == DRAGGLOW_GREEN);   // frame 1 in zone
    CHECK(DragGlowFor(true, true, false) == DRAGGLOW_GREEN);   // frame 2 still held -> same
    CHECK(DragGlowFor(true, false, false) == DRAGGLOW_NONE);   // left zone -> clear
}

int main() {
    test_pin_drag_in_zone_with_room_shows_green();
    test_full_dock_shows_red_on_entry_not_green_first();
    test_not_eager_nothing_before_entering_zone();
    test_unpin_reorder_or_rope_drag_never_glows();
    test_held_in_zone_is_sustained_then_clears_on_leave();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
