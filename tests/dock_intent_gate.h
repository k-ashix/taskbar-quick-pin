// Pure decision: while a real drag from a NON-dock source is in flight
// (DRAG_CANDIDATE), what should each polled frame do?
//
// Shared by mirroring (like the other tests/*.h) with the Windhawk mod
// (taskbar-quick-pin.wh.cpp, the DRAG_CANDIDATE block), with no Win32 deps.
//
// Refactor this pins (Stage 3 - Dock Intent):
// A new DRAG_CANDIDATE state sits between "real drag detected" and the
// expensive resolve. Each frame it does only a cheap dock-zone / taskbar
// hit-test (IsNearDockZone / IsCursorOverTaskbar). The behaviour contract:
//   * still holding, NO dock intent  -> keep waiting (stay in DRAG_CANDIDATE),
//     do NOT resolve;
//   * still holding, dock intent shown -> advance toward resolve/validation;
//   * button released while still a candidate (dock intent never shown, e.g.
//     dragging a window across the desktop) -> go straight to idle, NEVER
//     resolved.
//
// This is the guarantee that hovering/dragging near nothing costs no UIA /
// OpenProcess / icon-extraction work.

#ifndef TASKBAR_QUICK_PIN_DOCK_INTENT_GATE_H
#define TASKBAR_QUICK_PIN_DOCK_INTENT_GATE_H

enum CandidateAction {
    CANDIDATE_WAIT,     // still dragging, no dock intent yet -- do nothing costly
    CANDIDATE_ADVANCE,  // dock intent shown -- proceed to source validation/resolve
    CANDIDATE_ABORT     // released without ever showing dock intent -- go idle, no resolve
};

// buttonDown : is the drag button still held this frame?
// dockIntent : IsNearDockZone(cursor) || IsCursorOverTaskbar(cursor)
static inline CandidateAction NextCandidateAction(bool buttonDown, bool dockIntent) {
    if (!buttonDown) return CANDIDATE_ABORT;   // released as a mere candidate
    if (dockIntent)  return CANDIDATE_ADVANCE; // earns the expensive path
    return CANDIDATE_WAIT;                      // cheap idle-in-place
}

// Convenience: did this frame trigger any expensive resolution work?
// Only CANDIDATE_ADVANCE may lead to the resolver; WAIT and ABORT never do.
static inline bool CandidateTriggersResolve(CandidateAction a) {
    return a == CANDIDATE_ADVANCE;
}

#endif  // TASKBAR_QUICK_PIN_DOCK_INTENT_GATE_H
