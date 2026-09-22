// Pure decisions behind the drag-to-break feel, shared between the Windhawk mod
// (taskbar-quick-pin.wh.cpp) and its unit tests. No Win32 deps so the logic is
// verified in isolation.

#ifndef TASKBAR_QUICK_PIN_DRAG_FEEL_H
#define TASKBAR_QUICK_PIN_DRAG_FEEL_H

// Worker poll cadence (ms). Fast during any active/pending drag or animation so
// the cursor is sampled finely; medium while recently-idle; slow when long idle.
static const int DRAG_FAST_POLL_MS = 8;
static const int DRAG_MED_POLL_MS  = 16;
static const int DRAG_SLOW_POLL_MS = 50;

// Small over-stretch margin: the finite rope tears only once pulled a little
// PAST its break length, so a single boundary-straddling cursor sample can't
// fire an abrupt cut. A light polish, not a second break length.
static const float ROPE_BREAK_MARGIN_PX = 12.f;

// Drag states, mirrored from the mod's DragState enum (only the values these
// pure helpers need to reason about).
enum DragStateLite { DS_IDLE, DS_PRESS, DS_REORDER, DS_DRAGGING, DS_OTHER };

// How long the worker should sleep before the next frame.
//   * PRESS  -- mouse is down and we're watching for the move threshold; a quick
//     flick to tear an icon off must be sampled at the fast cadence or it reads
//     as a click (the miss-hit bug). So PRESS is fast, same as an active drag.
//   * DRAGGING / REORDER / any animation -- fast.
//   * recently idle (or still booting) -- medium; long idle -- slow.
static inline int DragWorkerPollMs(int state, bool animating,
                                   int idleFrames, bool notStable) {
    if (state == DS_PRESS || state == DS_DRAGGING || state == DS_REORDER || animating)
        return DRAG_FAST_POLL_MS;
    if (idleFrames < 10 || notStable)
        return DRAG_MED_POLL_MS;
    return DRAG_SLOW_POLL_MS;
}

// Whether the finite rope should tear this frame.
static inline bool RopeShouldBreak(float dist, float maxStretch,
                                   bool alreadyBreaking, bool dropZoneActive) {
    if (alreadyBreaking || dropZoneActive) return false;
    return dist >= maxStretch + ROPE_BREAK_MARGIN_PX;
}

#endif  // TASKBAR_QUICK_PIN_DRAG_FEEL_H
