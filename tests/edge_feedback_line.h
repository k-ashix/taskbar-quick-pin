// Pure, Win32-free decision for the drag-to-pin feedback glow (mirrored 1:1 in
// taskbar-quick-pin.wh.cpp).
//
// The dock's old right-edge feedback LINE was removed. Drag feedback now reuses
// the lock-glow bloom. This decides, per drag frame, WHICH glow to show:
//   * only for a PIN drag (a NEW app dragged IN) -- never an unpin / reorder /
//     rope-break drag of an already-pinned icon (pinningDrag == false),
//   * only while the cursor is INSIDE the dock drop zone (never "eager": nothing
//     shows before it enters),
//   * RED the moment it enters a FULL dock (pin limit) -- decided up front, so it
//     is never a green-then-red flash; GREEN while there is still room.
// The mod shows the result SUSTAINED while the item is held, and tears it down
// the instant this returns DRAGGLOW_NONE (left the zone, or released / dropped).

#ifndef TASKBAR_QUICK_PIN_EDGE_FEEDBACK_LINE_H
#define TASKBAR_QUICK_PIN_EDGE_FEEDBACK_LINE_H

enum DragGlow { DRAGGLOW_NONE, DRAGGLOW_GREEN, DRAGGLOW_RED };

// pinningDrag : a NEW app is being dragged in to pin (NOT an already-pinned icon
//               being dragged off to unpin / reorder / break its rope).
// inDropZone  : the cursor is currently inside the dock drop zone.
// dockFull    : the pin limit is already reached.
static inline DragGlow DragGlowFor(bool pinningDrag, bool inDropZone, bool dockFull) {
    if (!pinningDrag || !inDropZone) return DRAGGLOW_NONE;
    return dockFull ? DRAGGLOW_RED : DRAGGLOW_GREEN;
}

#endif  // TASKBAR_QUICK_PIN_EDGE_FEEDBACK_LINE_H
