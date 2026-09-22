// RED test (Prove-It) for the live "[DRAG] REJECT: no valid source" failure
// when trying to pin File Explorer as a workspace.
//
// Repro: pressing on a File Explorer window body (or its taskbar button) runs
// ResolveDragSourceAtPoint, which INTENTIONALLY returns "" for explorer.exe (so
// Explorer is never pinned as a normal app). The old gate,
// ShouldConfirmExplorerWorkspace(feature, resolvedExePath), keys purely on the
// resolved exe path being "explorer.exe" -- but that path is empty here, so the
// gate says "don't probe", the press falls through to the empty-path branch and
// logs REJECT: no valid source. The workspace pin can never start.
//
// The dock-freeze fix must still hold: a press that is NOT over an Explorer
// window (desktop, another app) must NOT run the expensive COM probe.
//
// Fix contract: gate on whether the pressed WINDOW is owned by explorer.exe,
// which the caller can determine cheaply (GetWindowThreadProcessId + module
// base name) WITHOUT the IShellWindows/UIA scan. So:
//   ShouldConfirmExplorerWorkspacePress(featureEnabled,
//                                       resolvedExePath,   // may be ""
//                                       pressedWindowIsExplorer)
//   probes when the feature is on AND (the resolve says explorer.exe
//   OR the pressed window is owned by explorer.exe).

#include "workspace_press_gate.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- The live bug: empty resolve over an Explorer-owned window --------------

static void test_empty_resolve_over_explorer_window_probes() {
    // resolve came back "" (Explorer blanked), but the window IS Explorer's.
    // Was false (-> REJECT: no valid source); must now probe.
    CHECK(ShouldConfirmExplorerWorkspacePress(true, L"", true) == true);
}

// ---- Dock-freeze guard still holds ------------------------------------------

static void test_empty_resolve_not_over_explorer_does_not_probe() {
    // Desktop / unknown: empty resolve AND not an Explorer window -> no probe.
    CHECK(ShouldConfirmExplorerWorkspacePress(true, L"", false) == false);
}

static void test_normal_app_press_does_not_probe() {
    CHECK(ShouldConfirmExplorerWorkspacePress(true, L"C:\\Windows\\System32\\notepad.exe", false) == false);
}

static void test_feature_off_never_probes() {
    CHECK(ShouldConfirmExplorerWorkspacePress(false, L"", true) == false);
    CHECK(ShouldConfirmExplorerWorkspacePress(false, L"C:\\Windows\\explorer.exe", true) == false);
}

// ---- Still probes on the paths that already worked --------------------------

static void test_explorer_path_resolve_still_probes() {
    CHECK(ShouldConfirmExplorerWorkspacePress(true, L"C:\\Windows\\explorer.exe", false) == true);
}

int main() {
    test_empty_resolve_over_explorer_window_probes();
    test_empty_resolve_not_over_explorer_does_not_probe();
    test_normal_app_press_does_not_probe();
    test_feature_off_never_probes();
    test_explorer_path_resolve_still_probes();

    if (g_failures == 0) {
        std::printf("workspace_press_gate_empty: ALL PASS\n");
        return 0;
    }
    std::printf("workspace_press_gate_empty: %d FAILURE(S)\n", g_failures);
    return 1;
}
