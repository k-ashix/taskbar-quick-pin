// Pure "has the taskbar changed enough that the tool must refresh?" decision.
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp, HasTaskbarGeometryChanged) and its unit tests
// (taskbar_lifecycle_test.cpp), with no Win32 deps.
//
// Why this exists (tool-mode lifecycle, roadmap spec 2):
//   As an Explorer-injected mod the process died and reloaded WITH explorer.exe,
//   so HasTaskbarGeometryChanged could assume Shell_TrayWnd always existed and
//   simply `return false` when FindWindow missed. As a TOOL MOD the process runs
//   in a dedicated windhawk.exe that OUTLIVES Explorer. Explorer can restart
//   while the tool stays alive, so:
//     * a vanished taskbar must be detected (so the stale dock is torn down), and
//     * a brand-new Shell_TrayWnd handle must be detected even if its rectangle
//       happens to equal the previous taskbar's.
//   The old code did neither, leaving the tool showing a stale/blank dock.
//
// This models the exact branch order of the fixed HasTaskbarGeometryChanged.

#ifndef TASKBAR_QUICK_PIN_TASKBAR_LIFECYCLE_H
#define TASKBAR_QUICK_PIN_TASKBAR_LIFECYCLE_H

// taskbarFound         : FindWindow(Shell_TrayWnd) succeeded this poll.
// sameHandleAsCached   : found HWND == g_cachedTaskbar (only meaningful when found).
// hadCachedTaskbar     : g_cachedTaskbar != NULL (we had a taskbar previously).
// haveLiveDockGeometry : g_dockLocalW > 0 (dock currently holds real geometry).
// trackedFieldsChanged : rect / Start-edge / screen-size comparison changed
//                        (only meaningful when found AND same handle).
//
// Contract (branch order matters):
//   1. taskbar gone      -> changed iff we still hold a cached taskbar OR live
//                           dock geometry (there is a stale dock to tear down).
//                           On a cold boot before any taskbar was seen, this is
//                           false (nothing to refresh yet).
//   2. new taskbar handle -> always changed (Explorer relaunched).
//   3. same taskbar       -> defer to the tracked-fields comparison.
static inline bool DecideTaskbarChanged(bool taskbarFound,
                                        bool sameHandleAsCached,
                                        bool hadCachedTaskbar,
                                        bool haveLiveDockGeometry,
                                        bool trackedFieldsChanged) {
    if (!taskbarFound) return hadCachedTaskbar || haveLiveDockGeometry;
    if (!sameHandleAsCached) return true;
    return trackedFieldsChanged;
}

// v2.5.3 -- Start-button loss during a taskbar rebuild / startup.
//
// HasTaskbarGeometryChanged used to treat only a MOVED Start button (a probe
// that SUCCEEDS with a new left edge) as a change. But if Shell_TrayWnd still
// exists while the Start button temporarily disappears (taskbar rebuild), the
// probe FAILS, the "moved" comparison is skipped, and nothing reports a change
// -- so RefreshTaskbarCache never runs and the previously placed dock geometry
// (which was anchored to the LEFT of a Start button that no longer resolves)
// survives as stale state.
//
// This predicate closes that gap: an unresolved Start is itself a change WHEN a
// dock is actually showing (haveLiveDockGeometry), so the cache is invalidated
// and the stale dock torn down. With no live dock yet (cold boot / still
// pending) there is nothing stale to refresh, so it is NOT a change.
//   startResolved        : GetStartButtonLeftEdge() succeeded this poll.
//   haveLiveDockGeometry : g_dockLocalW > 0 (a dock is currently placed).
static inline bool StartLossIsChange(bool startResolved, bool haveLiveDockGeometry) {
    return !startResolved && haveLiveDockGeometry;
}

#endif  // TASKBAR_QUICK_PIN_TASKBAR_LIFECYCLE_H
