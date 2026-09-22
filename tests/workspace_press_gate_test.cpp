// Unit tests for ShouldConfirmExplorerWorkspace: the gate that stops the
// expensive IShellWindows/UIA workspace probe from running on every press.
//
// Bug: the probe ran on the worker thread for every non-dock press when the
// workspace feature was on, stalling the dock. It must only run when the cheap
// resolve already says the pressed window is explorer.exe.

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

// ---- The bug: don't run the probe for non-Explorer presses ------------------

static void test_normal_app_press_does_not_probe() {
    // Was returning true (ran the COM scan on a Notepad click); must be false.
    CHECK(ShouldConfirmExplorerWorkspace(true, L"C:\\Windows\\System32\\notepad.exe") == false);
}

static void test_desktop_or_failed_resolve_does_not_probe() {
    CHECK(ShouldConfirmExplorerWorkspace(true, L"") == false);
}

// ---- Still works: probe for real Explorer presses ---------------------------

static void test_explorer_full_path_probes() {
    CHECK(ShouldConfirmExplorerWorkspace(true, L"C:\\Windows\\explorer.exe") == true);
}

static void test_explorer_bare_name_probes() {
    CHECK(ShouldConfirmExplorerWorkspace(true, L"explorer.exe") == true);
}

static void test_explorer_match_is_case_insensitive() {
    CHECK(ShouldConfirmExplorerWorkspace(true, L"C:\\Windows\\Explorer.EXE") == true);
}

static void test_forward_slash_path_probes() {
    CHECK(ShouldConfirmExplorerWorkspace(true, L"C:/Windows/explorer.exe") == true);
}

// ---- Feature switch dominates -----------------------------------------------

static void test_feature_off_never_probes_even_for_explorer() {
    CHECK(ShouldConfirmExplorerWorkspace(false, L"C:\\Windows\\explorer.exe") == false);
}

// ---- Guard against false positives ------------------------------------------

static void test_lookalike_name_does_not_probe() {
    CHECK(ShouldConfirmExplorerWorkspace(true, L"C:\\evil\\notexplorer.exe") == false);
    CHECK(ShouldConfirmExplorerWorkspace(true, L"C:\\evil\\explorer.exe.mal") == false);
}

int main() {
    test_normal_app_press_does_not_probe();
    test_desktop_or_failed_resolve_does_not_probe();
    test_explorer_full_path_probes();
    test_explorer_bare_name_probes();
    test_explorer_match_is_case_insensitive();
    test_forward_slash_path_probes();
    test_feature_off_never_probes_even_for_explorer();
    test_lookalike_name_does_not_probe();

    if (g_failures == 0) {
        std::printf("workspace_press_gate: ALL PASS\n");
        return 0;
    }
    std::printf("workspace_press_gate: %d FAILURE(S)\n", g_failures);
    return 1;
}
