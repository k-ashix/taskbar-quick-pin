// RED test (Prove-It pattern) for the "workspace explorer pin opens a duplicate
// window" instability.
//
// Repro of the live bug:
//   RestoreExplorerWindowGroup only feeds ONE folder -- the saved active tab --
//   into PickWorkspaceWindowToFocus. When that active tab was a *virtual*
//   Explorer location (This PC, Home, a library), FolderPathFromExplorer returns
//   "" at capture time, so the saved primary folder is empty. An empty primary
//   folder can never match an open window, so every click spawns ANOTHER
//   Explorer window for a workspace whose OTHER tabs are already open on disk.
//
// Fix contract (what this test pins):
//   PickWorkspaceWindowToFocusAny(candidateFolders, open) tries each candidate
//   folder in order and focuses the first open window that matches a NON-EMPTY
//   candidate -- so a workspace whose active tab is virtual but whose second tab
//   is "C:\Users\me\Documents" still focuses the already-open Documents window
//   instead of stacking a duplicate.

#include "workspace_focus.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- The bug: active tab is virtual (empty), a later tab is already open ----

static void test_skips_empty_active_folder_and_focuses_a_real_open_tab() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"C:\\Users\\me\\Documents" },
    };
    // Saved tab order: [0] = virtual "This PC" (empty), [1] = Documents.
    std::vector<std::wstring> candidates = { L"", L"C:\\Users\\me\\Documents" };
    // Was returning -1 (open a duplicate); must now focus index 0.
    CHECK(PickWorkspaceWindowToFocusAny(candidates, open) == 0);
}

static void test_all_candidates_empty_means_open_new() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"C:\\Users\\me\\Documents" },
    };
    std::vector<std::wstring> candidates = { L"", L"" };
    CHECK(PickWorkspaceWindowToFocusAny(candidates, open) == -1);
}

static void test_no_candidate_matches_open_windows_means_open_new() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"C:\\Games" },
    };
    std::vector<std::wstring> candidates = { L"", L"D:\\Work\\repo" };
    CHECK(PickWorkspaceWindowToFocusAny(candidates, open) == -1);
}

static void test_prefers_earliest_matching_candidate() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"D:\\Work\\repo" },
        { 200, L"C:\\Users\\me\\Documents" },
    };
    // Both are open; the first non-empty candidate that matches wins.
    std::vector<std::wstring> candidates = { L"C:\\Users\\me\\Documents",
                                             L"D:\\Work\\repo" };
    CHECK(PickWorkspaceWindowToFocusAny(candidates, open) == 1);
}

static void test_empty_candidate_list_means_open_new() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"C:\\Users\\me\\Documents" },
    };
    std::vector<std::wstring> candidates;
    CHECK(PickWorkspaceWindowToFocusAny(candidates, open) == -1);
}

int main() {
    test_skips_empty_active_folder_and_focuses_a_real_open_tab();
    test_all_candidates_empty_means_open_new();
    test_no_candidate_matches_open_windows_means_open_new();
    test_prefers_earliest_matching_candidate();
    test_empty_candidate_list_means_open_new();

    if (g_failures == 0) {
        std::printf("workspace_focus_multi: ALL PASS\n");
        return 0;
    }
    std::printf("workspace_focus_multi: %d FAILURE(S)\n", g_failures);
    return 1;
}
