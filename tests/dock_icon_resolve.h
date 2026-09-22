// Pure decision: given the pinned dock icon under the cursor, what source
// identity should Phase 0 of the drag/press resolver return?
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp, ResolveDragSourceZeroRejection Phase 0) and its
// unit tests (dock_icon_resolve_test.cpp), with no Win32 deps.
//
// Bug this pins ("opens once, then never again"):
// Phase 0 hit-tests the dock icons and returned g_pinnedApps[i].exePath. That
// is correct for an app pin, but a WORKSPACE pin carries a workspaceId and has
// an EMPTY exePath. So a press on a workspace dock icon returned "", which the
// resolver treats as "Phase 0 found nothing" -- it then falls through to the
// L1/L2/L3 layers, which all miss over the dock:
//
//     [DRAG] RESOLVER: dock icon
//     RESOLVER MISS: all layers failed near (784,1393)
//
// The workspace opened once (the click-launch path reads workspaceId by index),
// but every later press could not resolve, so it could never be opened or
// dragged again.
//
// Fix contract: a dock-icon hit must NEVER resolve to empty. A workspace pin
// resolves to the workspace sentinel the press path already uses
// ("Explorer workspace"); an app pin resolves to its exePath.

#ifndef TASKBAR_QUICK_PIN_DOCK_ICON_RESOLVE_H
#define TASKBAR_QUICK_PIN_DOCK_ICON_RESOLVE_H

#include <string>

// Mirrors the mod's PinType ordering (PIN_APP = 0, PIN_WORKSPACE = 1).
enum DockPinType { DOCK_PIN_APP = 0, DOCK_PIN_WORKSPACE = 1 };

// The sentinel the press path uses for an Explorer-workspace source
// (taskbar-quick-pin.wh.cpp: path = L"Explorer workspace").
inline const wchar_t* DockWorkspaceSentinel() { return L"Explorer workspace"; }

// Phase-0 dock-icon source identity. Returns:
//   * app pin       -> its exePath
//   * workspace pin -> the workspace sentinel (NEVER the empty exePath)
// The caller only invokes this for a confirmed icon hit, so the result is
// intended to be non-empty whenever the pin is well-formed.
static inline std::wstring ResolveDockIconSource(DockPinType type,
                                                 const std::wstring& exePath,
                                                 const std::wstring& workspaceId) {
    if (type == DOCK_PIN_WORKSPACE)
        return workspaceId.empty() ? std::wstring() : DockWorkspaceSentinel();
    return exePath;
}

#endif  // TASKBAR_QUICK_PIN_DOCK_ICON_RESOLVE_H
