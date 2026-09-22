// Pure decision: when the left button is released while the drag state machine
// is still in DRAG_PRESS (the move threshold was never crossed), is this a
// click that should launch the pinned icon?
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp, the PRESS->IDLE transition) and its unit tests
// (click_vs_drag_test.cpp), with no Win32 deps.
//
// Bug this pins ("clicks get consumed instead of action"):
// The mod decided wasClick with
//     elapsed < CLICK_MAX_MS (300ms) && dx <= 5 && dy <= 5
// But the drag state machine is driven by a POLLING loop (GetAsyncKeyState on
// 8/16/50ms sleeps), not by real mouse messages. When a window takes foreground
// after a launch, or the loop is briefly busy, the poll that observes the
// button-up can land well after 300ms even for a normal quick human click. Then
// wasClick was false, SmartLaunch was skipped, and the click vanished with no
// action -- for workspace pins AND app pins (Edge in the log resolved as a dock
// icon but never launched).
//
// The move threshold (DRAG_THRESHOLD_PX = 6) already separates a click from a
// drag: if the cursor had moved enough to be a drag, the state machine would
// have advanced to DRAG_DRAGGING and this code would never run. So a release in
// DRAG_PRESS with movement within the click box IS a click, regardless of how
// long the polling loop took to notice the release.
//
// Fix contract: wasClick depends on movement only, not on elapsed wall-clock
// time. The stationary-ness is bounded by the click move box.

#ifndef TASKBAR_QUICK_PIN_CLICK_VS_DRAG_H
#define TASKBAR_QUICK_PIN_CLICK_VS_DRAG_H

// Mirrors the mod's constant (pixels of movement still counted as a click).
#ifndef CLICK_MAX_MOVE_PX
#define CLICK_MAX_MOVE_PX 5
#endif

// True when a button-up in DRAG_PRESS should launch the icon under the cursor.
// Movement within the click box == a click. Elapsed time is intentionally NOT a
// factor: the polling loop can inflate it far beyond a human's intent.
//
// dx, dy: absolute cursor movement (px) since button-down.
static inline bool IsPressReleaseAClick(int dx, int dy) {
    return dx <= CLICK_MAX_MOVE_PX && dy <= CLICK_MAX_MOVE_PX;
}

#endif  // TASKBAR_QUICK_PIN_CLICK_VS_DRAG_H
