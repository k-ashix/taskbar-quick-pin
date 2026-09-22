// Pure workspace-id formatting for MakeWorkspaceId.
//
// Shared between the Windhawk mod (taskbar-quick-pin.wh.cpp) and its unit tests
// (workspace_id_test.cpp) so the id string layout is verified in isolation, with
// no Win32/COM deps.
//
// Functionality-note #10 (AI review): MakeWorkspaceId used
// GetTickCount() ^ time(NULL) for entropy, so two workspaces pinned in the same
// second on the same tick could collide. The fix uses a GUID (CoCreateGuid) for
// entropy; this header formats the GUID's 128 bits into the id string. The
// entropy source is not testable in isolation, but the formatting is -- these
// tests pin that a full 128-bit GUID is rendered (no bits dropped) and that the
// "workspace_" prefix / layout is stable.

#ifndef TASKBAR_QUICK_PIN_WORKSPACE_ID_H
#define TASKBAR_QUICK_PIN_WORKSPACE_ID_H

#include <string>
#include <cstdint>
#include <cwchar>

// Format the four 32-bit words of a GUID (Data1, Data2<<16|Data3, and the two
// halves of Data4) into a stable, collision-resistant workspace id:
//   workspace_XXXXXXXX_XXXXXXXX_XXXXXXXX_XXXXXXXX
// All 128 bits are rendered so distinct GUIDs always yield distinct ids.
static inline std::wstring FormatWorkspaceId(uint32_t w0, uint32_t w1,
                                             uint32_t w2, uint32_t w3) {
    wchar_t buf[64] = {};
    std::swprintf(buf, sizeof(buf) / sizeof(buf[0]),
                  L"workspace_%08X_%08X_%08X_%08X",
                  (unsigned)w0, (unsigned)w1, (unsigned)w2, (unsigned)w3);
    return buf;
}

#endif  // TASKBAR_QUICK_PIN_WORKSPACE_ID_H
