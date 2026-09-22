// Unit tests for the workspace focus-vs-open decision (PickWorkspaceWindowToFocus).
//
// Bug: clicking a pinned Explorer workspace always opened a NEW window, even
// when a window already showed that folder -> duplicate windows for the same
// doc. The app-pin path already focuses instead of relaunching; these tests pin
// the same contract for workspace pins.

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

// ---- The bug: focus an already-open workspace, do not open a duplicate ------

static void test_focuses_window_already_showing_the_folder() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"C:\\Users\\me\\Documents" },
    };
    // Was returning -1 (open duplicate); must now focus index 0.
    CHECK(PickWorkspaceWindowToFocus(L"C:\\Users\\me\\Documents", open) == 0);
}

static void test_opens_new_when_folder_not_open() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"C:\\Games" },
    };
    CHECK(PickWorkspaceWindowToFocus(L"C:\\Users\\me\\Documents", open) == -1);
}

static void test_match_is_case_insensitive() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"c:\\users\\ME\\documents" },
    };
    CHECK(PickWorkspaceWindowToFocus(L"C:\\Users\\me\\Documents", open) == 0);
}

static void test_match_ignores_trailing_backslash() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"C:\\Users\\me\\Documents\\" },
    };
    CHECK(PickWorkspaceWindowToFocus(L"C:\\Users\\me\\Documents", open) == 0);
}

static void test_picks_the_matching_window_among_several() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"C:\\Games" },
        { 200, L"D:\\Work\\repo" },
        { 300, L"C:\\Users\\me\\Documents" },
    };
    CHECK(PickWorkspaceWindowToFocus(L"D:\\Work\\repo", open) == 1);
}

// ---- Guard rails ------------------------------------------------------------

static void test_no_windows_open_means_open_new() {
    std::vector<OpenExplorerWindow> open;
    CHECK(PickWorkspaceWindowToFocus(L"C:\\Users\\me\\Documents", open) == -1);
}

static void test_empty_primary_folder_never_matches() {
    std::vector<OpenExplorerWindow> open = {
        { 100, L"" },
    };
    CHECK(PickWorkspaceWindowToFocus(L"", open) == -1);
}

static void test_zero_hwnd_is_ignored() {
    std::vector<OpenExplorerWindow> open = {
        { 0, L"C:\\Users\\me\\Documents" },
    };
    CHECK(PickWorkspaceWindowToFocus(L"C:\\Users\\me\\Documents", open) == -1);
}

static void test_bare_drive_letter_normalizes_to_root() {
    CHECK(WorkspaceFoldersMatch(L"C:", L"C:\\"));
}

int main() {
    test_focuses_window_already_showing_the_folder();
    test_opens_new_when_folder_not_open();
    test_match_is_case_insensitive();
    test_match_ignores_trailing_backslash();
    test_picks_the_matching_window_among_several();
    test_no_windows_open_means_open_new();
    test_empty_primary_folder_never_matches();
    test_zero_hwnd_is_ignored();
    test_bare_drive_letter_normalizes_to_root();

    if (g_failures == 0) {
        std::printf("workspace_focus: ALL PASS\n");
        return 0;
    }
    std::printf("workspace_focus: %d FAILURE(S)\n", g_failures);
    return 1;
}
