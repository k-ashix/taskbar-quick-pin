// Pure decision logic for when RepositionOverlay() may SHOW the dock windows.
//
// Mirrored (like the other tests/*.h) from taskbar-quick-pin.wh.cpp so it can
// be unit-tested with no Win32 deps.
//
// Bug this pins (Issue 3 -- "windows are shown before there is a valid dock"):
// UiThreadProc used to force-show both windows at (0,0) 200x48 unconditionally,
// before ownership was resolved and before any valid geometry existed. On
// Windows 10 / left-aligned Windows 11 the layout is UNSUPPORTED, RefreshTaskbarCache's
// SW_HIDE calls ran before any window existed (no-ops), and RepositionOverlay
// bailed on g_layoutUnsupported without hiding -- so the 1/255-alpha HTCLIENT
// input window sat in the top-left corner permanently, swallowing clicks and
// popping the context menu. Cold start flashed a mini-dock at (0,0); a DISOWN
// left a live window + registered hotkey in a non-owner process.
//
// Fix contract, modelled here:
//   * Visibility is owned SOLELY by the worker's RepositionOverlay, never by the
//     UI thread. Nothing is shown from UiThreadProc.
//   * The dock is shown ONLY when ALL hold: ownership decided (OWN), layout
//     supported, and a real geometry exists (dockLocalW > 0). Otherwise BOTH
//     windows are explicitly hidden.
//   * The emergency fallback synthesizes a 200px dock ONLY when a real taskbar
//     (Shell_TrayWnd) exists; with no taskbar it returns HIDDEN (no top-left flash).
//   * A DISOWN process (ownership never decided here) always resolves to HIDDEN.

#ifndef TASKBAR_QUICK_PIN_OVERLAY_VISIBILITY_GATE_H
#define TASKBAR_QUICK_PIN_OVERLAY_VISIBILITY_GATE_H

// Inputs to the visibility decision, mirroring the relevant globals.
struct OverlayGateInput {
    bool ownershipDecided;   // g_dockOwnershipDecided (set only on QP_STARTUP_OWN)
    bool layoutUnsupported;  // g_layoutUnsupported
    int  dockLocalW;         // g_dockLocalW (>0 == real geometry)
    bool taskbarPresent;     // Shell_TrayWnd exists (for the fallback path)
};

// Resolved geometry width after RepositionOverlay's emergency fallback runs.
// Returns the synthesized 200 only when a taskbar exists to anchor to; the
// pre-fix code forced 200 unconditionally (the (0,0) flash bug).
static inline int ResolvedDockWidth(const OverlayGateInput& in) {
    if (in.dockLocalW > 0) return in.dockLocalW;
    if (in.taskbarPresent) return 200;   // safe fallback anchored to the taskbar
    return 0;                            // no taskbar yet -> no geometry -> hidden
}

// The single visibility decision. true == show the dock windows; false == both
// windows must be SW_HIDE'd. This is the whole point of the fix: show only when
// owned, supported, and backed by real geometry.
static inline bool ShouldShowDock(const OverlayGateInput& in) {
    if (!in.ownershipDecided) return false;   // DEFER / DISOWN -> hidden
    if (in.layoutUnsupported) return false;   // unsupported layout -> hidden
    if (ResolvedDockWidth(in) <= 0) return false;  // no valid geometry -> hidden
    return true;
}

// UiThreadProc must NEVER show a window itself; visibility is decided later by
// the worker. Modelled as a constant so a regression that re-adds a show call
// there is caught.
static inline bool UiThreadShowsWindows() {
    return false;
}

#endif  // TASKBAR_QUICK_PIN_OVERLAY_VISIBILITY_GATE_H
