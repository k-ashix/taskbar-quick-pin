// Pure taskbar-layout decision math for the Quick Pin dock.
//
// Shared between the Windhawk mod (taskbar-quick-pin.wh.cpp) and its unit tests
// (taskbar_layout_test.cpp) so the "is there room left of Start, and what should
// the state machine do about it" decision is verified in isolation, with no
// Win32 deps.
//
// Background (the bug this fixes -- roadmap Issue 2):
//   RefreshTaskbarCache historically bailed with
//       if (newW <= 0 || startLeft <= tbr.left + 50) return;
//   On a LEFT-ALIGNED taskbar (Win11 "Align: Left", and all Win10) the Start
//   button sits at ~tbr.left + 16, so `startLeft <= tbr.left + 50` is TRUE on
//   every call. The function returned BEFORE the state machine ran, so
//   g_systemState never left STATE_BOOT: the dock was clamped to x=0 on top of
//   Start (eating clicks) and the worker polled at 100 ms forever.
//
// The decision below replaces that single early-return with three explicit
// outcomes, so the caller can ALWAYS drive the state machine to a terminal
// (idle) state instead of spinning:
//
//   LAYOUT_OK          -> Start is far enough right that the fixed-width dock
//                         fits to its left. Proceed with normal geometry.
//   LAYOUT_UNSUPPORTED -> Start is detected but hugs the left edge (left-aligned
//                         taskbar): there is no room for the dock. Hide it and
//                         settle the state machine to idle -- do NOT keep polling.
//   LAYOUT_PENDING     -> Geometry isn't usable yet (bad/zero taskbar rect or a
//                         nonsensical Start edge): keep booting / retry.

#ifndef TASKBAR_QUICK_PIN_TASKBAR_LAYOUT_H
#define TASKBAR_QUICK_PIN_TASKBAR_LAYOUT_H

enum LayoutDecision {
    LAYOUT_PENDING = 0,   // not enough info yet -- keep the boot poll running
    LAYOUT_OK,            // room to the left of Start -- place the dock normally
    LAYOUT_UNSUPPORTED    // left-aligned / no room -- hide dock, settle to idle
};

// tbrLeft, tbrRight : taskbar window rect horizontal bounds (screen px).
// startLeft         : detected Start-button left edge (screen px).
// dockGapPx         : gap between the dock's right edge and Start.
// dockWidth         : the fixed dock width to be placed left of Start.
// minRoom           : minimum usable dock width to consider the layout OK
//                     (mirror of MIN_VALID_DOCK_WIDTH in the mod).
//
// Contract:
//   * A degenerate taskbar rect (width <= 0) or a Start edge outside the taskbar
//     is PENDING -- we can't trust it, so the caller should keep retrying rather
//     than commit to a wrong layout or a permanent "unsupported".
//   * Otherwise the available room to the left of Start is
//         room = (startLeft - dockGapPx) - tbrLeft
//     If that room can hold at least min(dockWidth, minRoom) pixels, it's OK;
//     if Start hugs the left edge so the room is smaller than that, it's the
//     UNSUPPORTED left-aligned layout.
static inline LayoutDecision DecideTaskbarLayout(long tbrLeft, long tbrRight,
                                                 long startLeft,
                                                 int dockGapPx, int dockWidth,
                                                 int minRoom) {
    // Degenerate / not-ready taskbar geometry.
    if (tbrRight <= tbrLeft) return LAYOUT_PENDING;
    // Start edge must be a sane value strictly inside the taskbar. A Start edge
    // at/left-of tbrLeft, or at/right-of tbrRight, means detection hasn't
    // produced a trustworthy value yet.
    if (startLeft <= tbrLeft || startLeft >= tbrRight) return LAYOUT_PENDING;

    if (dockGapPx < 0) dockGapPx = 0;

    // How many horizontal pixels are available to the left of Start (after the
    // gap) before we would run off the left edge of the taskbar.
    long room = (startLeft - (long)dockGapPx) - tbrLeft;
    if (room < 0) room = 0;

    // The dock only needs as much room as it actually is; but never demand more
    // than minRoom to call it OK (a very small dock still needs a sane minimum).
    int need = dockWidth < minRoom ? dockWidth : minRoom;
    if (need < 1) need = 1;

    return (room >= need) ? LAYOUT_OK : LAYOUT_UNSUPPORTED;
}

#endif  // TASKBAR_QUICK_PIN_TASKBAR_LAYOUT_H
