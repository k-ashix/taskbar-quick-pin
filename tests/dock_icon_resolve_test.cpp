// Unit tests for ResolveDockIconSource: Phase 0 of the drag/press resolver must
// never return empty for a hit dock icon.
//
// Bug (from the field log): a workspace dock icon resolved to "" (its empty
// exePath), so the resolver fell through to L1/L2/L3, all missed over the dock
// ("RESOLVER MISS: all layers failed"), and the workspace pin could be opened
// only once -- never again.

#include "dock_icon_resolve.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- The bug: a workspace dock icon must resolve to a non-empty source ------

static void test_workspace_pin_resolves_to_sentinel_not_empty() {
    // exePath is empty for a workspace pin; it must resolve via workspaceId.
    std::wstring src = ResolveDockIconSource(
        DOCK_PIN_WORKSPACE, /*exePath=*/L"",
        /*workspaceId=*/L"workspace_0D9CCCAE_B7844CDC_88230DDC_9B5CCF10");
    CHECK(!src.empty());
    CHECK(src == DockWorkspaceSentinel());
}

static void test_workspace_pin_resolves_after_first_open() {
    // Regression for "opens once then never again": the second resolve of the
    // same workspace icon must still be non-empty (nothing is consumed).
    const std::wstring wsId = L"workspace_ABC";
    std::wstring first  = ResolveDockIconSource(DOCK_PIN_WORKSPACE, L"", wsId);
    std::wstring second = ResolveDockIconSource(DOCK_PIN_WORKSPACE, L"", wsId);
    CHECK(first == DockWorkspaceSentinel());
    CHECK(second == DockWorkspaceSentinel());
}

// ---- Still works: an app dock icon resolves to its exePath ------------------

static void test_app_pin_resolves_to_exe_path() {
    std::wstring src = ResolveDockIconSource(
        DOCK_PIN_APP, L"C:\\AI_Agents\\Antigravity_IDE\\Antigravity IDE.exe", L"");
    CHECK(src == L"C:\\AI_Agents\\Antigravity_IDE\\Antigravity IDE.exe");
}

// ---- Degenerate guard: a malformed workspace pin (no id) stays empty --------

static void test_workspace_pin_without_id_is_empty() {
    // Nothing to launch; empty is correct so the resolver rejects it rather than
    // opening a bogus workspace.
    CHECK(ResolveDockIconSource(DOCK_PIN_WORKSPACE, L"", L"").empty());
}

int main() {
    test_workspace_pin_resolves_to_sentinel_not_empty();
    test_workspace_pin_resolves_after_first_open();
    test_app_pin_resolves_to_exe_path();
    test_workspace_pin_without_id_is_empty();

    if (g_failures == 0) {
        std::printf("dock_icon_resolve: ALL PASS\n");
        return 0;
    }
    std::printf("dock_icon_resolve: %d FAILURE(S)\n", g_failures);
    return 1;
}
