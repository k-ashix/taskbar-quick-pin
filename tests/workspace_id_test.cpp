// Unit tests for workspace-id formatting (functionality note #10). The old
// MakeWorkspaceId used GetTickCount() ^ time(NULL), so two workspaces created in
// the same second on the same tick collided. The fix renders a full 128-bit
// GUID. These tests pin the formatting contract:
//
//   * The "workspace_" prefix and 4-word layout are stable.
//   * All 128 bits are rendered -- two GUIDs differing in ANY word (including
//     ones that would collide under the old tick^time scheme) produce distinct
//     ids.

#include "workspace_id.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_layout_is_stable() {
    std::wstring id = FormatWorkspaceId(0x01234567u, 0x89ABCDEFu,
                                        0xDEADBEEFu, 0xFEEDFACEu);
    CHECK(id == L"workspace_01234567_89ABCDEF_DEADBEEF_FEEDFACE");
}

static void test_zero_guid_padded() {
    // Leading zeros must be preserved (8 hex digits per word).
    std::wstring id = FormatWorkspaceId(0, 0, 0, 0);
    CHECK(id == L"workspace_00000000_00000000_00000000_00000000");
}

static void test_distinct_in_each_word() {
    std::wstring base = FormatWorkspaceId(1, 2, 3, 4);
    CHECK(FormatWorkspaceId(9, 2, 3, 4) != base);  // word 0 differs
    CHECK(FormatWorkspaceId(1, 9, 3, 4) != base);  // word 1 differs
    CHECK(FormatWorkspaceId(1, 2, 9, 4) != base);  // word 2 differs
    CHECK(FormatWorkspaceId(1, 2, 3, 9) != base);  // word 3 differs
}

static void test_no_collision_across_full_range() {
    // High bits must not be dropped: two ids that differ only in the top bit of
    // the last word were the exact class the old scheme could collide on.
    CHECK(FormatWorkspaceId(0, 0, 0, 0x80000000u) !=
          FormatWorkspaceId(0, 0, 0, 0x00000000u));
}

int main() {
    test_layout_is_stable();
    test_zero_guid_padded();
    test_distinct_in_each_word();
    test_no_collision_across_full_range();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
