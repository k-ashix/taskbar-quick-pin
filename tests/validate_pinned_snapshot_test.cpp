// Unit tests for the ValidateAndCleanPinnedList snapshot split (review Optional
// item: don't hold g_cs across LoadWorkspaceSnapshot storage I/O).
//
// Contract:
//   * Phase 1 (under lock) copies out one descriptor per entry and does NO
//     storage I/O -- it only reads in-memory fields and copies the workspace id.
//   * App-pin validity is fully decided in phase 1 (no I/O needed).
//   * Phase 2 (outside lock) is the ONLY place a storage lookup happens, and it
//     needs only the copied id -- so it can run with the lock released.

#include "validate_pinned_snapshot.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static std::vector<PinnedEntryLite> sampleList() {
    return {
        PinnedEntryLite{PK_APP,       true,  L"C:\\a.exe", L""},        // valid app
        PinnedEntryLite{PK_APP,       false, L"C:\\b.exe", L""},        // no icon -> invalid
        PinnedEntryLite{PK_APP,       true,  L"",          L""},        // empty path -> invalid
        PinnedEntryLite{PK_WORKSPACE, true,  L"",          L"ws_123"},  // needs lookup
        PinnedEntryLite{PK_WORKSPACE, true,  L"",          L""},        // empty id -> no lookup
    };
}

static void test_snapshot_covers_every_entry_in_order() {
    auto d = SnapshotForValidation(sampleList());
    CHECK(d.size() == 5);
    for (int i = 0; i < (int)d.size(); ++i) CHECK(d[i].index == i);
}

static void test_workspace_id_is_copied_under_lock() {
    // The id must be copied in phase 1 so phase 2 needs no locked field.
    auto d = SnapshotForValidation(sampleList());
    CHECK(d[3].kind == PK_WORKSPACE);
    CHECK(d[3].workspaceId == L"ws_123");
}

static void test_app_validity_decided_in_phase1_no_io() {
    auto d = SnapshotForValidation(sampleList());
    CHECK(d[0].appDecidedInvalid == false);  // valid app
    CHECK(d[1].appDecidedInvalid == true);   // no icon
    CHECK(d[2].appDecidedInvalid == true);   // empty path
}

static void test_only_nonempty_workspace_pins_need_a_lookup() {
    auto d = SnapshotForValidation(sampleList());
    CHECK(!NeedsStorageLookup(d[0]));  // app
    CHECK( NeedsStorageLookup(d[3]));  // workspace w/ id
    CHECK(!NeedsStorageLookup(d[4]));  // workspace, empty id
}

int main() {
    test_snapshot_covers_every_entry_in_order();
    test_workspace_id_is_copied_under_lock();
    test_app_validity_decided_in_phase1_no_io();
    test_only_nonempty_workspace_pins_need_a_lookup();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
