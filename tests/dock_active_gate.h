// Pure decision: is the dock currently ACTIVE (on screen) and therefore allowed
// to run any drag / resolve / dock-zone / pin-unpin / lock-glow work this frame?
//
// Shared by mirroring (like the other tests/*.h) with the Windhawk mod
// (taskbar-quick-pin.wh.cpp, the dock-inactive gate at the head of the drag
// state machine in WorkerThread), with no Win32 deps.
//
// Why this exists (v2.5.3):
//   The overlay window is SUPPRESSED (kept hidden) for several reasons -- a
//   fullscreen / exclusive app or a secure snip overlay, an unsupported
//   (left-aligned) layout, a missing taskbar while Explorer restarts, or
//   geometry that has not resolved yet (zero width / an empty cached rect).
//   Hiding the overlay was not enough: the worker still fell through to the drag
//   state machine and could resolve sources, detect dock zones, and evaluate
//   pin/unpin against the STALE cached dock geometry -- background work against a
//   dock the user cannot even see. A hidden dock must be a NON-INTERACTIVE dock.
//
// Contract: interaction is allowed ONLY when every suppression condition is
// clear. If ANY is set the dock is down and the whole drag pipeline must be
// skipped for the frame (after tearing down anything mid-flight).

#ifndef TASKBAR_QUICK_PIN_DOCK_ACTIVE_GATE_H
#define TASKBAR_QUICK_PIN_DOCK_ACTIVE_GATE_H

// fullscreenActive  : g_fullscreenActive (fullscreen app / snip overlay owns screen)
// layoutUnsupported : g_layoutUnsupported (left-aligned / no room left of Start)
// haveCachedTaskbar : g_cachedTaskbar != NULL (we currently track a taskbar)
// dockLocalW        : g_dockLocalW (live dock client width; <= 0 == no geometry)
// cachedRectValid   : g_cachedDockRect is a real, non-empty rectangle
// autoHideHidden    : "Sync with taskbar auto-hide" is ON and the taskbar has
//                     slid off screen, so RepositionOverlay has hidden the dock
//                     even though the cached rect is still valid (Issue 3).
static inline bool DockInteractionAllowed(bool fullscreenActive,
                                          bool layoutUnsupported,
                                          bool haveCachedTaskbar,
                                          int  dockLocalW,
                                          bool cachedRectValid,
                                          bool autoHideHidden) {
    if (fullscreenActive)   return false;   // fullscreen app / snip overlay
    if (layoutUnsupported)  return false;   // left-aligned / no room
    if (!haveCachedTaskbar) return false;   // taskbar gone (Explorer restarting)
    if (dockLocalW <= 0)    return false;   // geometry not resolved / zero width
    if (!cachedRectValid)   return false;   // cached dock rect is empty/invalid
    if (autoHideHidden)     return false;   // auto-hidden off screen -> dock not visible
    return true;
}

#endif  // TASKBAR_QUICK_PIN_DOCK_ACTIVE_GATE_H
