// Pure accept-distance logic for the Layer-3 process-fallback resolver.
//
// Shared between the Windhawk mod (taskbar-quick-pin.wh.cpp) and its unit tests
// (layer3_accept_test.cpp) so the "is this window a valid drop target for the
// cursor" decision is verified in isolation, with no Win32 deps.
//
// Functionality-note #1 (AI review): Resolver_Layer3_ProcessFallback picked the
// visible top-level window whose rect was nearest the cursor within 50 px. So a
// press on the desktop or on a window's edge could hand back a NEIGHBOURING
// app's path, and dragging that to the dock pinned the wrong thing. The fix is
// to require the cursor to actually be INSIDE the window (dist == 0) for this
// fallback to accept it -- edge/near matches are rejected.
//
// The distance metric is the Manhattan gap from the point to the window rect:
// 0 when the point is inside, otherwise dx + dy where dx/dy are how far the
// point lies outside the rect horizontally/vertically. This mirrors the
// enumerator's own computation so the two stay in lockstep.

#ifndef TASKBAR_QUICK_PIN_LAYER3_ACCEPT_H
#define TASKBAR_QUICK_PIN_LAYER3_ACCEPT_H

// Manhattan gap from a point (px,py) to an axis-aligned rect [l,t,r,b).
// Returns 0 when the point is inside the rect (inclusive of the edges the way
// PtInRect treats them), otherwise the sum of the horizontal and vertical
// overflow. Never negative.
static inline int Layer3PointRectDistance(int px, int py,
                                          int l, int t, int r, int b) {
    int dx = 0, dy = 0;
    if (px < l)      dx = l - px;
    else if (px > r) dx = px - r;
    if (py < t)      dy = t - py;
    else if (py > b) dy = py - b;
    return dx + dy;
}

// The accept test for the Layer-3 fallback. A candidate window is only a valid
// pin target when the cursor is actually inside it (dist == 0). Anything merely
// near the window (an edge hit, a neighbouring app) is rejected so the fallback
// can't resolve to an unrelated app.
static inline bool Layer3AcceptsDistance(int dist) {
    return dist == 0;
}

#endif  // TASKBAR_QUICK_PIN_LAYER3_ACCEPT_H
