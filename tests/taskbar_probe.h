// Pure "where else should L2 probe when the exact taskbar point misses" logic.
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp, Resolver_Layer2_TaskbarIntelligence retry) and its
// unit tests (taskbar_probe_test.cpp), with no Win32 deps.
//
// Bug this pins ("RESOLVER MISS near (1066,1391) even though a taskbar button
// is right there, mostly when windhawk.exe is foreground"):
//
//   14:10:47 RESOLVER: dock icon | ...Antigravity IDE.exe
//   14:10:47 RESOLVER MISS: all layers failed near (1066,1391)   <- taskbar path
//
// When the cursor is over the taskbar, the resolver is UIAutomation-only with NO
// L1/L3 fallback (taskbar-authority invariant). UIA ElementFromPoint at the EXACT
// pixel can return the taskbar container (explorer.exe) instead of a button when
// the click lands in the ~1-3px SEAM between adjacent taskbar buttons, or a hair
// above/below the button's clickable sub-rect. That yields an empty L2 result and
// an immediate MISS -- no retry -- so the action is dropped. It shows up "mostly
// when windhawk is foreground" because a foreground app subtly shifts which
// element is topmost at the exact seam pixel.
//
// Fix contract: on an exact-point L2 miss over the taskbar, retry UIA
// ElementFromPoint at a SMALL ring of nudged probe points around the original,
// snapping toward a real button center. The probes:
//   * stay UIA-only (still taskbar-authoritative; never consult foreground/active
//     window or process enum),
//   * are bounded to at most half a taskbar-button pitch away, so we never jump
//     onto a NEIGHBORING button,
//   * are ordered nearest-first (vertical-center first, then small horizontal
//     nudges) so the closest genuine button wins,
//   * are omitted entirely if the button pitch is unknown (<= 0) -> no retry.

#ifndef TASKBAR_QUICK_PIN_TASKBAR_PROBE_H
#define TASKBAR_QUICK_PIN_TASKBAR_PROBE_H

#include <vector>

struct ProbePoint { int x; int y; };

// Build the ordered list of L2 retry probe points for an exact-point miss at
// (missX, missY).
//   buttonPitchPx : center-to-center spacing of taskbar buttons (e.g. ~44). The
//                   max horizontal nudge is clamped to < half a pitch so a probe
//                   can never reach a neighboring button's center.
//   taskbarMidY   : vertical center line of the taskbar-button row. Probes snap
//                   Y to this so a click slightly above/below the button still
//                   resolves. If <= 0, Y is left unchanged.
// Returns nearest-first probes and NEVER includes the original (missX, missY)
// itself (already tried). Empty when buttonPitchPx <= 0 (unknown geometry).
static inline std::vector<ProbePoint> BuildTaskbarProbes(int missX, int missY,
                                                         int buttonPitchPx,
                                                         int taskbarMidY) {
    std::vector<ProbePoint> probes;
    if (buttonPitchPx <= 0) return probes;  // unknown geometry -> no retry

    // Largest horizontal nudge that still stays on the SAME button: strictly
    // less than half a pitch. Use a third of the pitch as a safe inner ring.
    int hStep = buttonPitchPx / 3;
    if (hStep < 1) hStep = 1;
    int hMax = (buttonPitchPx / 2) - 1;
    if (hMax < 1) hMax = 1;

    int snappedY = (taskbarMidY > 0) ? taskbarMidY : missY;

    // 1) Same X, snapped to the button vertical center (fixes above/below seam).
    if (snappedY != missY)
        probes.push_back({ missX, snappedY });

    // 2) Small horizontal nudges (both directions), nearest first, snapped Y.
    for (int dx = hStep; dx <= hMax; dx += hStep) {
        probes.push_back({ missX - dx, snappedY });
        probes.push_back({ missX + dx, snappedY });
    }

    return probes;
}

#endif  // TASKBAR_QUICK_PIN_TASKBAR_PROBE_H
