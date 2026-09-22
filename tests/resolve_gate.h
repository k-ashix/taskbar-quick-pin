// Pure decision: the whole 5-stage lazy-resolution gate collapsed into one
// function -- does a given drag earn EXACTLY ONE expensive resolution, and only
// when every stage passes?
//
// Shared by mirroring (like the other tests/*.h) with the Windhawk mod
// (taskbar-quick-pin.wh.cpp, mouse-down classification + DRAG_CANDIDATE gate),
// with no Win32 deps.
//
// Models the guarantee described in the refactor:
//   Stage 1 (press classification): a DOCK-icon source (fromDock) is the cheap
//           internal path -- it structurally never reaches DRAG_CANDIDATE, so it
//           never resolves. Only a non-dock source becomes a candidate.
//   Stage 2 (real drag): must exceed the OS drag threshold, else it's a click.
//   Stage 3 (dock intent): must show dock intent while held, else it aborts idle.
//   Stage 4 (source validation): candidate window must be live + non-system.
//   Stage 5 (resolve): runs iff all the above hold -- and at most ONCE per drag.
//
// ResolveCount() returns how many times the expensive resolver would fire for a
// single drag gesture: 0 (gated out) or 1 (earned). Never more than 1.

#ifndef TASKBAR_QUICK_PIN_RESOLVE_GATE_H
#define TASKBAR_QUICK_PIN_RESOLVE_GATE_H

// A single drag gesture described by the observable facts of each stage.
struct DragGesture {
    bool fromDock;           // Stage 1: source was a dock icon (internal path)
    bool exceededThreshold;  // Stage 2: real drag (vs click)
    bool showedDockIntent;   // Stage 3: cursor entered dock zone / taskbar while held
    bool candidateLive;      // Stage 4: recorded window still valid (IsWindow)
    bool candidateSystem;    // Stage 4: recorded window is a shell/system surface
};

// True iff this gesture should run the one expensive resolution (Stage 5).
static inline bool ShouldResolve(const DragGesture& g) {
    if (g.fromDock)            return false; // internal path never resolves
    if (!g.exceededThreshold)  return false; // a click, not a drag
    if (!g.showedDockIntent)   return false; // dragged away, aborted idle
    if (!g.candidateLive)      return false; // stale window -> fail closed
    if (g.candidateSystem)     return false; // shell surface -> fail closed
    return true;
}

// The resolver fires at most once per gesture: 1 when earned, else 0.
static inline int ResolveCount(const DragGesture& g) {
    return ShouldResolve(g) ? 1 : 0;
}

#endif  // TASKBAR_QUICK_PIN_RESOLVE_GATE_H
