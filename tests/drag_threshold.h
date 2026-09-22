// Pure decision: has the pointer moved far enough since mouse-down to count as
// a REAL drag (leaving DRAG_PRESS)?
//
// Shared by mirroring (like the other tests/*.h) with the Windhawk mod
// (taskbar-quick-pin.wh.cpp, the PRESS -> DRAG_REORDER/DRAG_CANDIDATE transition),
// with no Win32 deps.
//
// Refactor this pins (Stage 2 - Real Drag Detection):
// The mod used to compare movement against a single fixed DRAG_THRESHOLD_PX
// (6px) guess. It now uses the OS-configured drag thresholds
//     s_dragCX = GetSystemMetrics(SM_CXDRAG)
//     s_dragCY = GetSystemMetrics(SM_CYDRAG)
// cached once. The contract that matters (and that we can test without Win32):
//   * the threshold is PER-AXIS: dx is compared to CX, dy is compared to CY;
//   * it is a strict "exceeds" test (moving exactly to the threshold is still a
//     press, not yet a drag) -- matching the mod's "> threshold" crossing;
//   * crossing EITHER axis is enough to call it a drag.
//
// SM_CXDRAG / SM_CYDRAG can differ per axis and per machine, so the decision is
// parameterised on the metrics rather than hard-coding 6.

#ifndef TASKBAR_QUICK_PIN_DRAG_THRESHOLD_H
#define TASKBAR_QUICK_PIN_DRAG_THRESHOLD_H

// True when movement (dx,dy) since mouse-down exceeds the OS drag thresholds on
// either axis -- i.e. the state machine should leave DRAG_PRESS for a real drag.
//
// dx, dy: absolute pointer movement (px) since button-down (non-negative).
// cxDrag, cyDrag: GetSystemMetrics(SM_CXDRAG/SM_CYDRAG), cached by the caller.
static inline bool ExceedsDragThreshold(int dx, int dy, int cxDrag, int cyDrag) {
    return dx > cxDrag || dy > cyDrag;
}

#endif  // TASKBAR_QUICK_PIN_DRAG_THRESHOLD_H
