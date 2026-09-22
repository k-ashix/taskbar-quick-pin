// Pure content sub-rect bounds math for the rope/vanish layered surfaces.
//
// Shared between the Windhawk mod (taskbar-quick-pin.wh.cpp) and its unit tests
// (subrect_bounds_test.cpp) so the geometry that decides which pixels to clear
// and premultiply each frame is verified in isolation, with no Win32/GDI deps.
//
// Contract: content is anchored at the buffer top-left, so a content extent of
// (cw, ch) occupies buffer rect [0,cw) x [0,ch). The returned rect always
// covers BOTH the current extent and the previous frame's extent (so a shrinking
// effect never leaves stale pixels behind on the fixed, re-presented surface),
// and is clamped to the surface so the memset/premultiply/plot loops stay in
// bounds.

#ifndef TASKBAR_QUICK_PIN_SUBRECT_BOUNDS_H
#define TASKBAR_QUICK_PIN_SUBRECT_BOUNDS_H

struct ClearBounds {
    int x;   // always 0 (content anchored top-left) -- kept explicit for callers
    int y;   // always 0
    int w;   // columns to clear/premultiply, in [0, surfW]
    int h;   // rows to clear/premultiply,    in [0, surfH]
};

// curW/curH  : this frame's content extent (cw/ch or vanish box W/H).
// prevW/prevH: last frame's content extent on the same surface (0 if none).
// surfW/surfH: the fixed surface dimensions.
static inline ClearBounds ComputeClearBounds(int curW, int curH,
                                             int prevW, int prevH,
                                             int surfW, int surfH) {
    int w = curW > prevW ? curW : prevW;   // union of this + last frame
    int h = curH > prevH ? curH : prevH;
    if (w < 0) w = 0;                      // degenerate inputs -> empty, never negative
    if (h < 0) h = 0;
    if (w > surfW) w = surfW;              // never exceed the fixed surface
    if (h > surfH) h = surfH;
    ClearBounds b{ 0, 0, w, h };
    return b;
}

#endif  // TASKBAR_QUICK_PIN_SUBRECT_BOUNDS_H
