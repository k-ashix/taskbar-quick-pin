// Pure decision: WHICH screen point does Stage 5 resolve at?
//
// Shared by mirroring (like the other tests/*.h) with the Windhawk mod
// (taskbar-quick-pin.wh.cpp, the DRAG_CANDIDATE -> resolve step), with no Win32
// deps.
//
// Bug this pins (the one fixed in this pass):
// Stage 5 used to resolve the drag source at the LIVE cursor:
//     ResolveDragSourceAtPoint(cursor)
//     IsExplorerWorkspaceDragSource(cursor, ...)
// But dock intent fires when the cursor is over the dock/taskbar -- so it
// resolved whatever taskbar button the cursor happened to hover, not the window
// the user actually grabbed. Grabbing an Explorer window and dragging it to the
// dock would resolve the wrong app (or a taskbar button).
//
// Fix contract: Stage 5 resolves at the RECORDED mouse-down point
// (g_dragStartPt), captured at PRESS time, regardless of where the cursor is
// when dock intent fires. Both call sites (ResolveDragSourceAtPoint and
// IsExplorerWorkspaceDragSource) must use this same recorded point.
//
// (GetProcessPath(candidateWnd) was already correct: candidateWnd was captured
// at press time, so it is unaffected. This header models only the POINT choice.)

#ifndef TASKBAR_QUICK_PIN_RESOLVE_AT_START_POINT_H
#define TASKBAR_QUICK_PIN_RESOLVE_AT_START_POINT_H

struct TqpPoint {
    int x;
    int y;
};

// Returns the point Stage 5 must resolve at. By contract this is ALWAYS the
// recorded drag-start point, never the live cursor.
//
// dragStartPt : g_dragStartPt, recorded at mouse-down (PRESS).
// liveCursor  : where the cursor is now (when dock intent fired). Deliberately
//               unused for the decision -- passed only to make the wrong old
//               behaviour expressible/guardable in tests.
static inline TqpPoint ResolvePoint(TqpPoint dragStartPt, TqpPoint liveCursor) {
    (void)liveCursor;   // must NOT influence the result
    return dragStartPt;
}

static inline bool PointsEqual(TqpPoint a, TqpPoint b) {
    return a.x == b.x && a.y == b.y;
}

#endif  // TASKBAR_QUICK_PIN_RESOLVE_AT_START_POINT_H
