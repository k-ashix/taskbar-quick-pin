// Pure snapshot logic for ValidateAndCleanPinnedList (review Optional item:
// "ValidateAndCleanPinnedList holds g_cs across storage I/O").
//
// Shared between the Windhawk mod (taskbar-quick-pin.wh.cpp) and its unit tests
// (validate_pinned_snapshot_test.cpp) so the "which entries need a storage read,
// and can that read happen outside the lock?" decision is verified with no Win32
// deps.
//
// Bug this pins: the validator held g_cs (the pinned-list lock) for the WHOLE
// pass, and called LoadWorkspaceSnapshot -- which does Wh_GetIntValue /
// Wh_GetStringValue storage I/O -- for every workspace pin WHILE the lock was
// held. That blocks the worker thread (reorder, reposition) on disk/registry
// latency for as long as the validation runs.
//
// Fix contract: split the pass into two phases.
//   Phase 1 (UNDER lock): snapshot a small, self-contained descriptor of every
//     entry -- its index, type, and (for workspace pins) the id string to look
//     up. App-pin validity depends only on in-memory fields, so it is decided
//     here too. NO storage I/O happens under the lock.
//   Phase 2 (OUTSIDE lock): for each workspace descriptor, do the
//     LoadWorkspaceSnapshot storage read. Only the id string -- copied in
//     phase 1 -- is needed, so the lock is not required.

#ifndef TASKBAR_QUICK_PIN_VALIDATE_PINNED_SNAPSHOT_H
#define TASKBAR_QUICK_PIN_VALIDATE_PINNED_SNAPSHOT_H

#include <string>
#include <vector>

enum PinKind { PK_APP, PK_WORKSPACE };

// Minimal stand-in for a pinned entry (the fields the validator reads).
struct PinnedEntryLite {
    PinKind      kind;
    bool         hasIcon;      // g_pinnedApps[i].icon != NULL
    std::wstring exePath;      // app pins
    std::wstring workspaceId;  // workspace pins
};

// A descriptor captured UNDER the lock in phase 1. It carries everything phase 2
// needs, so no locked field is touched during the storage read.
struct ValidationDescriptor {
    int          index;
    PinKind      kind;
    std::wstring workspaceId;  // populated only for PK_WORKSPACE
    bool         appDecidedInvalid;  // app-pin verdict, decided under lock
};

// Phase 1: called while the lock is held. Copies out everything phase 2 needs.
// Must NOT perform any storage I/O.
static inline std::vector<ValidationDescriptor>
SnapshotForValidation(const std::vector<PinnedEntryLite>& pins) {
    std::vector<ValidationDescriptor> out;
    out.reserve(pins.size());
    for (int i = 0; i < (int)pins.size(); ++i) {
        const PinnedEntryLite& e = pins[i];
        ValidationDescriptor d{};
        d.index = i;
        d.kind  = e.kind;
        if (e.kind == PK_WORKSPACE) {
            d.workspaceId = e.workspaceId;                 // copy the id string
            d.appDecidedInvalid = false;                   // decided in phase 2
        } else {
            // App-pin validity uses only in-memory fields -> decide here.
            d.appDecidedInvalid = !e.hasIcon || e.exePath.empty();
        }
        out.push_back(d);
    }
    return out;
}

// Phase 2 helper: does this descriptor still need a storage read? Only workspace
// pins do -- and their id was already copied, so the lock is not needed.
static inline bool NeedsStorageLookup(const ValidationDescriptor& d) {
    return d.kind == PK_WORKSPACE && !d.workspaceId.empty();
}

#endif  // TASKBAR_QUICK_PIN_VALIDATE_PINNED_SNAPSHOT_H
