// Pure gating decision for the bare-key P/U/L triple-tap gestures.
//
// Shared between the Windhawk mod (taskbar-quick-pin.wh.cpp) and its unit tests
// (key_gesture_gating_test.cpp) so the "should the worker evaluate the keyboard
// gesture this tick?" decision is verified in isolation, with no Win32 deps.
//
// Bug this pins: the keyboard-gesture block used to live AFTER the worker loop's
//   if (!g_overlayWnd || !IsWindow(g_overlayWnd)) { ...; continue; }
// guard. The P/U/L gestures act on GetForegroundWindow() and do NOT need the
// dock's overlay window to exist -- but because they sat behind that guard, when
// the overlay never came up (e.g. the dock stuck in STATE_BOOT on some taskbar
// layouts) the keys silently did nothing even with the setting enabled. The fix
// moves the block ABOVE the overlay guard, so the ONLY thing that gates the
// keyboard gesture is whether the user turned it on.
//
// Contract: the keyboard gesture is evaluated iff the feature is enabled. It is
// deliberately INDEPENDENT of overlay-window presence (unlike the rapid-click
// unpin-all gesture, which hit-tests dock icons and legitimately requires it).

#ifndef TASKBAR_QUICK_PIN_KEY_GESTURE_GATING_H
#define TASKBAR_QUICK_PIN_KEY_GESTURE_GATING_H

// keyGesturesEnabled : the ENABLE_KEY_GESTURES user setting.
// Returns true when the worker should run the P/U/L rising-edge detection this
// tick. Note the absence of any overlay/window parameter -- that is the point.
static inline bool ShouldEvalKeyGesture(bool keyGesturesEnabled) {
    return keyGesturesEnabled;
}

// The rapid-click unpin-all gesture DOES need the dock window + geometry (it
// hit-tests icon rects), so it stays gated on both the setting and the overlay.
static inline bool ShouldEvalRapidClick(bool rapidUnpinEnabled,
                                        bool overlayWindowExists) {
    return rapidUnpinEnabled && overlayWindowExists;
}

#endif  // TASKBAR_QUICK_PIN_KEY_GESTURE_GATING_H
