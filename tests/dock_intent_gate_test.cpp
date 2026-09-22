// Unit tests for NextCandidateAction (Stage 3 - Dock Intent gate).
//
// Guards the DRAG_CANDIDATE contract: a real non-dock drag does NOTHING
// expensive until the cursor shows dock intent, and a drag released without
// dock intent (window dragged across the desktop) is never resolved.

#include "dock_intent_gate.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_holding_without_dock_intent_waits() {
    // The whole point: dragging near nothing costs nothing.
    CHECK(NextCandidateAction(/*buttonDown=*/true, /*dockIntent=*/false) == CANDIDATE_WAIT);
    CHECK(CandidateTriggersResolve(CANDIDATE_WAIT) == false);
}

static void test_dock_intent_advances_to_resolve() {
    CHECK(NextCandidateAction(true, true) == CANDIDATE_ADVANCE);
    CHECK(CandidateTriggersResolve(CANDIDATE_ADVANCE) == true);
}

static void test_release_without_dock_intent_aborts_never_resolves() {
    // Dragging a window across the desktop and dropping it far from the dock.
    CHECK(NextCandidateAction(/*buttonDown=*/false, /*dockIntent=*/false) == CANDIDATE_ABORT);
    CHECK(CandidateTriggersResolve(CANDIDATE_ABORT) == false);
}

static void test_release_is_abort_even_if_over_dock_on_the_up_frame() {
    // Contract: release ends the candidate. Even if dock intent happens to be
    // true on the same frame the button goes up, releasing means "let go",
    // handled by the drop path -- the candidate frame itself aborts, it does
    // not kick off a fresh resolve.
    CHECK(NextCandidateAction(false, true) == CANDIDATE_ABORT);
    CHECK(CandidateTriggersResolve(CANDIDATE_ABORT) == false);
}

static void test_only_advance_ever_triggers_resolve() {
    CHECK(CandidateTriggersResolve(CANDIDATE_WAIT)    == false);
    CHECK(CandidateTriggersResolve(CANDIDATE_ABORT)   == false);
    CHECK(CandidateTriggersResolve(CANDIDATE_ADVANCE) == true);
}

int main() {
    test_holding_without_dock_intent_waits();
    test_dock_intent_advances_to_resolve();
    test_release_without_dock_intent_aborts_never_resolves();
    test_release_is_abort_even_if_over_dock_on_the_up_frame();
    test_only_advance_ever_triggers_resolve();

    if (g_failures == 0) {
        std::printf("dock_intent_gate: ALL PASS\n");
        return 0;
    }
    std::printf("dock_intent_gate: %d FAILURE(S)\n", g_failures);
    return 1;
}
