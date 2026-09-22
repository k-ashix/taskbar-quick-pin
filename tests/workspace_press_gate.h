// Pure gate: on a mouse-press, should the EXPENSIVE Explorer-workspace probe run?
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp) and its unit tests (workspace_press_gate_test.cpp),
// so the decision is verified with no Win32/COM deps.
//
// Bug this pins: in the DRAG_IDLE -> PRESS transition, the mod evaluated
//     workspaceSource = ENABLE_EXPLORER_WORKSPACE_PINS &&
//                       IsExplorerWorkspaceDragSource(cursor, ...)
// FIRST, before the cheap hover-candidate / ResolveDragSourceAtPoint fast path.
// IsExplorerWorkspaceDragSource does a full IShellWindows enumerate + a UI
// Automation ElementFromPoint, synchronously, on the worker thread that also
// animates and repaints the dock. So EVERY press on any window/taskbar/desktop
// paid for that COM scan and the dock stalled ("frozen top layer"). Turning the
// workspace feature off skipped the whole branch, so it felt smooth -- which is
// how the feature was identified as the culprit.
//
// Fix contract: resolve the cheap source identity first, then only run the
// expensive probe when that identity is explorer.exe (both ways to start a
// workspace pin -- the Explorer window body and its taskbar button -- resolve to
// explorer.exe). An empty/failed resolve never triggers the probe.

#ifndef TASKBAR_QUICK_PIN_WORKSPACE_PRESS_GATE_H
#define TASKBAR_QUICK_PIN_WORKSPACE_PRESS_GATE_H

#include <string>
#include <cwctype>

// Lower-cased final path component (after the last '\' or '/').
static inline std::wstring PressGateBaseNameLower(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    for (wchar_t& c : name)
        c = (wchar_t)towlower((wint_t)c);
    return name;
}

// True only when the expensive Explorer-workspace probe is worth running:
// the feature is enabled AND the cheap resolve already identified the pressed
// window as explorer.exe. Anything else (empty resolve, a normal app) returns
// false, so the COM scan never runs on that press.
static inline bool ShouldConfirmExplorerWorkspace(bool featureEnabled,
                                                  const std::wstring& resolvedExePath) {
    return featureEnabled &&
           PressGateBaseNameLower(resolvedExePath) == L"explorer.exe";
}

// Refined gate for the DRAG_IDLE -> PRESS transition.
//
// Live bug ("[DRAG] REJECT: no valid source" when pinning File Explorer):
// ResolveDragSourceAtPoint deliberately returns "" for explorer.exe, so keying
// only on `resolvedExePath == explorer.exe` (the function above) meant a genuine
// Explorer press -- whose resolve is empty -- was never probed and fell through
// to the empty-path reject. The pin could never start.
//
// The caller can cheaply learn whether the window under the cursor is owned by
// explorer.exe (GetWindowThreadProcessId + the process module's base name) with
// NO IShellWindows/UIA scan. Probe when the feature is on AND either signal says
// Explorer:
//   * the cheap resolve already named explorer.exe, OR
//   * the pressed window is owned by explorer.exe (covers the empty-resolve case).
// A press that is neither (desktop, another app) still skips the expensive COM
// probe, so the dock-freeze fix that motivated ShouldConfirmExplorerWorkspace
// still holds.
static inline bool ShouldConfirmExplorerWorkspacePress(bool featureEnabled,
                                                       const std::wstring& resolvedExePath,
                                                       bool pressedWindowIsExplorer) {
    if (!featureEnabled)
        return false;
    return pressedWindowIsExplorer ||
           PressGateBaseNameLower(resolvedExePath) == L"explorer.exe";
}

#endif  // TASKBAR_QUICK_PIN_WORKSPACE_PRESS_GATE_H
