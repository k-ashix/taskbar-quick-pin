// Pure "what layout decision follows from the Start-button probe" logic.
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp) and its unit tests (start_edge_probe_test.cpp),
// with no Win32 deps.
//
// Bug this pins (roadmap Issue 1B):
//   GetStartButtonLeftEdge used to SUBSTITUTE a fabricated estimate --
//       tbRect.left + (tbRect.right - tbRect.left) / 5
//   -- whenever the real Start window could not be found. QpDecideTaskbarLayout
//   then treated that made-up 1/5-width edge as a genuine reading and could
//   commit to an OK/UNSUPPORTED layout for a taskbar whose Start position was
//   never actually resolved, instead of staying PENDING and retrying.
//
// Fix contract: the probe reports Start explicitly as found / not-found. A
// NOT-FOUND probe must yield NO edge, so the layout decision is PENDING (keep
// booting / retry) -- never a fabricated geometry. A FOUND edge is passed
// straight to DecideTaskbarLayout, which alone classifies OK vs UNSUPPORTED
// (including a legitimately left-aligned Start that hugs the left edge). The
// probe must NOT pre-reject a found-but-left-edge Start, or it would regress the
// left-aligned detection back into a permanent PENDING spin.

#ifndef TASKBAR_QUICK_PIN_START_EDGE_PROBE_H
#define TASKBAR_QUICK_PIN_START_EDGE_PROBE_H

#include "taskbar_layout.h"  // LayoutDecision + DecideTaskbarLayout (already tested)

// startFound        : true iff GetStartButtonLeftEdge located a real Start /
//                     InputSite child (fast path in-bounds, or the enum on the
//                     taskbar's left half).
// measuredStartLeft : that window's left edge in screen px (meaningful only when
//                     startFound is true).
// tbrLeft, tbrRight : taskbar window rect horizontal bounds (screen px).
// gap, dockW, minRoom: same params fed to DecideTaskbarLayout in the mod.
//
// Contract:
//   * !startFound  -> LAYOUT_PENDING, ALWAYS. We never invent an edge, so a
//                     taskbar whose Start we could not resolve keeps booting.
//   * startFound   -> DecideTaskbarLayout(...) decides OK / UNSUPPORTED / PENDING
//                     from the real measured edge.
static inline LayoutDecision DecideLayoutFromStartProbe(bool startFound,
                                                        long measuredStartLeft,
                                                        long tbrLeft, long tbrRight,
                                                        int gap, int dockW,
                                                        int minRoom) {
    if (!startFound) return LAYOUT_PENDING;
    return DecideTaskbarLayout(tbrLeft, tbrRight, measuredStartLeft,
                               gap, dockW, minRoom);
}

#endif  // TASKBAR_QUICK_PIN_START_EDGE_PROBE_H
