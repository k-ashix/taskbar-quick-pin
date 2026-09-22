// Pure "focus an already-open workspace vs. open a new window" decision.
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp) and its unit tests (workspace_focus_test.cpp), so
// the click-to-launch decision is verified with no Win32/COM deps.
//
// Bug this pins: SmartLaunch's app-pin path focuses an already-running app
// (FindRunningAppWindow -> SetForegroundWindow) instead of launching a
// duplicate, but the workspace-pin path had no such guard -- LaunchWorkspace ->
// RestoreExplorerWindowGroup always ran `explorer.exe /n,"<folder>"`, so every
// click on a pinned workspace opened ANOTHER Explorer window for the SAME
// folder. The user sees the same doc stack into a second window.
//
// Fix contract: before opening, look at the currently-open Explorer windows. If
// one already shows the workspace's active/primary folder, focus THAT window and
// skip the launch. Comparison is path-normalized and case-insensitive, because
// Explorer reports paths with inconsistent drive-letter/trailing-slash/case
// forms.

#ifndef TASKBAR_QUICK_PIN_WORKSPACE_FOCUS_H
#define TASKBAR_QUICK_PIN_WORKSPACE_FOCUS_H

#include <string>
#include <vector>
#include <cstdint>
#include <cwctype>

// One currently-open Explorer window: an opaque window id (HWND as integer;
// 0 means "none") plus the folder path it is currently showing.
struct OpenExplorerWindow {
    std::intptr_t hwndId;
    std::wstring  folder;
};

// Normalize a folder path for workspace-identity comparison:
//   * forward slashes -> backslash
//   * bare drive letter gets its separator ("C:docs" -> "C:\docs")
//   * a single trailing separator is stripped, EXCEPT a bare root ("C:\" stays)
//   * lower-cased (Windows paths are case-insensitive)
static inline std::wstring NormalizeForCompare(std::wstring p) {
    for (wchar_t& c : p)
        if (c == L'/') c = L'\\';

    if (p.size() >= 2 &&
        ((p[0] >= L'A' && p[0] <= L'Z') || (p[0] >= L'a' && p[0] <= L'z')) &&
        p[1] == L':') {
        if (p.size() == 2)               // "C:"    -> "C:\"
            p += L"\\";
        else if (p[2] != L'\\')          // "C:docs" -> "C:\docs"
            p = p.substr(0, 2) + L"\\" + p.substr(2);
    }

    while (p.size() > 3 && p.back() == L'\\')
        p.pop_back();

    for (wchar_t& c : p)
        c = (wchar_t)towlower((wint_t)c);

    return p;
}

// True when two folder paths refer to the same folder.
static inline bool WorkspaceFoldersMatch(const std::wstring& a,
                                         const std::wstring& b) {
    return NormalizeForCompare(a) == NormalizeForCompare(b);
}

// Decide the click action for a workspace pin whose active/primary folder is
// `primaryFolder`, given the currently-open Explorer windows.
//   returns >= 0 : index into `open` of the window to FOCUS (already open)
//   returns  -1  : no match, OPEN a new window
static inline int PickWorkspaceWindowToFocus(
        const std::wstring& primaryFolder,
        const std::vector<OpenExplorerWindow>& open) {
    if (primaryFolder.empty())
        return -1;
    for (int i = 0; i < (int)open.size(); ++i) {
        if (open[i].hwndId != 0 &&
            WorkspaceFoldersMatch(open[i].folder, primaryFolder))
            return i;
    }
    return -1;
}

// Like PickWorkspaceWindowToFocus, but tries several candidate folders in order
// (typically a workspace group's tab folders, active tab first). Empty
// candidates are skipped -- a virtual Explorer location (This PC, Home, a
// library) resolves to "" at capture time, and an empty folder must never be
// treated as "already open" or it would suppress the restore entirely. Returns
// the index into `open` of the first window matching the earliest NON-EMPTY
// candidate, or -1 when nothing matches (open a new window).
static inline int PickWorkspaceWindowToFocusAny(
        const std::vector<std::wstring>& candidateFolders,
        const std::vector<OpenExplorerWindow>& open) {
    for (const std::wstring& folder : candidateFolders) {
        if (folder.empty())
            continue;
        int idx = PickWorkspaceWindowToFocus(folder, open);
        if (idx >= 0)
            return idx;
    }
    return -1;
}

#endif  // TASKBAR_QUICK_PIN_WORKSPACE_FOCUS_H
