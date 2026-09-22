// Pure decision: is the recorded drag-source window still eligible to be
// resolved (Stage 4 - Source Validation), or must we fail closed?
//
// Shared by mirroring (like the other tests/*.h) with the Windhawk mod
// (taskbar-quick-pin.wh.cpp, the guard right before ResolveDragSourceAtPoint:
//     candidateWnd && IsWindow(candidateWnd) && !IsSystemWindow(candidateWnd) ),
// with no Win32 deps.
//
// Refactor this pins (Stage 4 - Source Validation):
// Before doing any expensive resolution, the mod re-checks the window it
// recorded at PRESS time (g_dragCandidateWindow = GetRealWindowFromPoint). It
// must:
//   * fail closed on a null handle (nothing was captured);
//   * fail closed on a stale handle (window gone by the time we drop -- IsWindow
//     is false);
//   * fail closed on a system/shell surface (taskbar, Start, shell -- these can
//     never be pinned), so the resolver is never even called for them.
// Only a live, non-system window may proceed to Stage 5.
//
// The Win32 predicates (IsWindow / IsSystemWindow) are collapsed here into
// plain booleans the caller supplies, so the DECISION is testable without Win32.

#ifndef TASKBAR_QUICK_PIN_SOURCE_VALIDATION_H
#define TASKBAR_QUICK_PIN_SOURCE_VALIDATION_H

// hasHandle    : candidateWnd != NULL (something was captured at press time)
// windowStillLive : IsWindow(candidateWnd) -- false if it was closed mid-drag
// isSystemWindow  : IsSystemWindow(candidateWnd) -- taskbar/Start/shell surface
//
// Returns true only when it is safe to run the expensive resolver.
static inline bool SourcePassesValidation(bool hasHandle,
                                          bool windowStillLive,
                                          bool isSystemWindow) {
    if (!hasHandle)        return false; // nothing recorded -> fail closed
    if (!windowStillLive)  return false; // stale/destroyed  -> fail closed
    if (isSystemWindow)    return false; // shell surface    -> fail closed
    return true;
}

#endif  // TASKBAR_QUICK_PIN_SOURCE_VALIDATION_H
