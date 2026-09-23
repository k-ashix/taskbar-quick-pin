// Unit tests for the "stay calm when the dragged icon is ALREADY pinned"
// decision (pin_limit_calm.h), composed with the drag-to-pin feedback glow
// (edge_feedback_line.h).
//
// The gap this closes: dragging an app that is ALREADY pinned toward the dock
// used to be treated as a genuine new-pin attempt. On a FULL dock that raised
// the RED "dock full" rejection bloom (and the dock shake); on a dock with room
// it raised the GREEN "will add" bloom -- even though dropping an already-pinned
// app is a harmless no-op (PinApp dedups it before the cap check). The contract
// these tests pin:
//
//   * IsNewPinFeedbackDrag(fromDock, alreadyPinned) is true ONLY for a genuine
//     new pin: an EXTERNAL drag (fromDock == false) of an app that is NOT
//     already pinned. Reordering / pulling-off a dock icon (fromDock == true),
//     or dropping an already-pinned app back on the dock (alreadyPinned), is
//     NOT a new pin.
//   * Composed with DragGlowFor, an already-pinned icon dragged near/into the
//     dock stays CALM (DRAGGLOW_NONE) in BOTH limit states -- dock full or with
//     room -- while a genuine new pin still shows GREEN (room) / RED (full).
//
// Self-contained (own main()); auto-discovered and compiled by run_tests.ps1.

#include "pin_limit_calm.h"
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

// ---- the pure decision truth table ----

static void test_external_new_app_is_a_new_pin() {
    // External drag (not from the dock) of an app that isn't pinned yet.
    CHECK(IsNewPinFeedbackDrag(/*fromDock=*/false, /*alreadyPinned=*/false) == true);
}

static void test_external_already_pinned_is_not_a_new_pin() {
    // Dropping an already-pinned app back on the dock is a no-op -> not a new pin.
    CHECK(IsNewPinFeedbackDrag(/*fromDock=*/false, /*alreadyPinned=*/true) == false);
}

static void test_dock_icon_reorder_or_unpin_is_never_a_new_pin() {
    // Dragging / deleting an existing dock icon (reorder, or pull-off to unpin).
    CHECK(IsNewPinFeedbackDrag(/*fromDock=*/true, /*alreadyPinned=*/true)  == false);
    CHECK(IsNewPinFeedbackDrag(/*fromDock=*/true, /*alreadyPinned=*/false) == false);
}

// ---- end-to-end: composed with the feedback glow, calm in BOTH limit states ----

static void test_already_pinned_stays_calm_when_dock_full() {
    // limit HIT: no RED "dock full" rejection for an already-pinned icon.
    bool pinningDrag = IsNewPinFeedbackDrag(/*fromDock=*/false, /*alreadyPinned=*/true);
    CHECK(DragGlowFor(pinningDrag, /*inDropZone=*/true, /*dockFull=*/true) == DRAGGLOW_NONE);
}

static void test_already_pinned_stays_calm_when_dock_has_room() {
    // limit NOT hit: no GREEN "will add" bloom either -> calm in both states.
    bool pinningDrag = IsNewPinFeedbackDrag(/*fromDock=*/false, /*alreadyPinned=*/true);
    CHECK(DragGlowFor(pinningDrag, /*inDropZone=*/true, /*dockFull=*/false) == DRAGGLOW_NONE);
}

static void test_reorder_or_unpin_stays_calm_in_both_states() {
    // Dock-icon drag/delete: calm whether the dock is full or has room.
    bool pinningDrag = IsNewPinFeedbackDrag(/*fromDock=*/true, /*alreadyPinned=*/true);
    CHECK(DragGlowFor(pinningDrag, /*inDropZone=*/true, /*dockFull=*/true)  == DRAGGLOW_NONE);
    CHECK(DragGlowFor(pinningDrag, /*inDropZone=*/true, /*dockFull=*/false) == DRAGGLOW_NONE);
}

static void test_genuine_new_pin_still_gives_feedback() {
    // Regression guard: a real new pin still shows GREEN (room) / RED (full), so
    // this change does not weaken legitimate pin-limit feedback.
    bool pinningDrag = IsNewPinFeedbackDrag(/*fromDock=*/false, /*alreadyPinned=*/false);
    CHECK(DragGlowFor(pinningDrag, /*inDropZone=*/true, /*dockFull=*/false) == DRAGGLOW_GREEN);
    CHECK(DragGlowFor(pinningDrag, /*inDropZone=*/true, /*dockFull=*/true)  == DRAGGLOW_RED);
}

int main() {
    test_external_new_app_is_a_new_pin();
    test_external_already_pinned_is_not_a_new_pin();
    test_dock_icon_reorder_or_unpin_is_never_a_new_pin();
    test_already_pinned_stays_calm_when_dock_full();
    test_already_pinned_stays_calm_when_dock_has_room();
    test_reorder_or_unpin_stays_calm_in_both_states();
    test_genuine_new_pin_still_gives_feedback();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
