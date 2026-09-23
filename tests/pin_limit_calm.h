// Pure, Win32-free decision: is a drag toward the dock a genuine NEW-PIN attempt
// for feedback purposes? Mirrored 1:1 into taskbar-quick-pin.wh.cpp (used at the
// DRAG_DRAGGING drag-glow site, feeding DragGlowFor's `pinningDrag` input).
//
// The drag-to-pin feedback glow -- GREEN "will add" while the dock has room, RED
// "dock full" (pin limit) rejection with a shake when it does not -- must fire
// ONLY when the drag actually adds a NEW pin. Two "already-pinned" gestures must
// instead STAY CALM (no glow, no shake, no rejection), in BOTH limit states
// (dock full or with room):
//   * dragging / deleting an ALREADY-PINNED dock icon -- reorder, or pull-off to
//     unpin (fromDock == true); and
//   * dropping an app that is ALREADY pinned back onto the dock, which PinApp
//     dedups into a harmless no-op before the cap check (alreadyPinned == true).
// The only genuine new pin left is an EXTERNAL drag of an app that is not
// already pinned.

#ifndef TASKBAR_QUICK_PIN_PIN_LIMIT_CALM_H
#define TASKBAR_QUICK_PIN_PIN_LIMIT_CALM_H

// fromDock      : the drag started on an existing dock icon (reorder / unpin),
//                 as opposed to an external drag-to-pin from another app.
// alreadyPinned : the dragged app is already in the pin list, so re-dropping it
//                 would be a no-op.
static inline bool IsNewPinFeedbackDrag(bool fromDock, bool alreadyPinned) {
    return !fromDock && !alreadyPinned;
}

#endif  // TASKBAR_QUICK_PIN_PIN_LIMIT_CALM_H
