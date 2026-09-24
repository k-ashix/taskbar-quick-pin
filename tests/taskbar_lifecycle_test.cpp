// Unit tests for the tool-mode taskbar lifecycle decision (roadmap spec 2:
// Explorer can restart while the dedicated tool process stays alive).
//
// The old Explorer-injected HasTaskbarGeometryChanged did `if (!tb) return false`
// and never compared the taskbar HANDLE, so as a tool mod it would (a) keep a
// stale dock when Explorer/taskbar disappeared and (b) miss a fresh Shell_TrayWnd
// that reused the previous rectangle. DecideTaskbarChanged pins the fixed
// branch order.

#include "taskbar_lifecycle.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- Taskbar disappeared (Explorer restarting) -----------------------------

static void test_taskbar_lost_with_cached_taskbar_is_change() {
    // Had a taskbar; it's gone now -> must refresh (tear the stale dock down).
    CHECK(DecideTaskbarChanged(/*found*/false, /*sameHandle*/false,
                               /*hadCached*/true, /*haveGeom*/true,
                               /*tracked*/false) == true);
}

static void test_taskbar_lost_with_geometry_only_is_change() {
    // Defensive: even if the cached handle was cleared but we still show dock
    // geometry, a lost taskbar is a change.
    CHECK(DecideTaskbarChanged(/*found*/false, /*sameHandle*/false,
                               /*hadCached*/false, /*haveGeom*/true,
                               /*tracked*/false) == true);
}

static void test_cold_boot_no_taskbar_yet_is_not_a_change() {
    // Cold start / sign-in before any taskbar was ever seen: nothing to refresh.
    CHECK(DecideTaskbarChanged(/*found*/false, /*sameHandle*/false,
                               /*hadCached*/false, /*haveGeom*/false,
                               /*tracked*/false) == false);
}

// ---- Taskbar present --------------------------------------------------------

static void test_new_shell_traywnd_handle_is_change_even_if_rect_matches() {
    // Explorer relaunched: new HWND, tracked fields "unchanged" (same rect) must
    // still be reported as a change so geometry re-resolves against the new bar.
    CHECK(DecideTaskbarChanged(/*found*/true, /*sameHandle*/false,
                               /*hadCached*/true, /*haveGeom*/true,
                               /*tracked*/false) == true);
}

static void test_same_taskbar_tracked_change_propagates() {
    CHECK(DecideTaskbarChanged(/*found*/true, /*sameHandle*/true,
                               /*hadCached*/true, /*haveGeom*/true,
                               /*tracked*/true) == true);
}

static void test_same_taskbar_no_tracked_change_is_no_change() {
    CHECK(DecideTaskbarChanged(/*found*/true, /*sameHandle*/true,
                               /*hadCached*/true, /*haveGeom*/true,
                               /*tracked*/false) == false);
}

// ---- Start-button loss during a taskbar rebuild (v2.5.3) --------------------
// Same Shell_TrayWnd, same rect, but the Start-button probe FAILED this poll
// (Start temporarily gone while the taskbar rebuilds). The dock anchors to the
// LEFT of Start, so an unresolved Start means the currently shown geometry is
// stale and must be torn down -- but only when a dock is actually showing.

static void test_start_lost_with_live_dock_is_change() {
    // Dock showing + Start gone -> stale geometry -> must refresh/tear down.
    CHECK(StartLossIsChange(/*startResolved=*/false, /*haveLiveDockGeometry=*/true) == true);
}

static void test_start_lost_without_live_dock_is_not_change() {
    // Cold boot / still pending: no dock geometry to invalidate yet.
    CHECK(StartLossIsChange(/*startResolved=*/false, /*haveLiveDockGeometry=*/false) == false);
}

static void test_start_resolved_is_not_a_loss_change() {
    // Start resolved fine: the loss path never fires. A MOVED Start is handled by
    // the normal tracked-field comparison, not by this predicate.
    CHECK(StartLossIsChange(/*startResolved=*/true, /*haveLiveDockGeometry=*/true) == false);
    CHECK(StartLossIsChange(/*startResolved=*/true, /*haveLiveDockGeometry=*/false) == false);
}

int main() {
    test_taskbar_lost_with_cached_taskbar_is_change();
    test_taskbar_lost_with_geometry_only_is_change();
    test_cold_boot_no_taskbar_yet_is_not_a_change();
    test_new_shell_traywnd_handle_is_change_even_if_rect_matches();
    test_same_taskbar_tracked_change_propagates();
    test_same_taskbar_no_tracked_change_is_no_change();
    test_start_lost_with_live_dock_is_change();
    test_start_lost_without_live_dock_is_not_change();
    test_start_resolved_is_not_a_loss_change();

    if (g_failures == 0) {
        std::printf("taskbar_lifecycle: ALL PASS\n");
        return 0;
    }
    std::printf("taskbar_lifecycle: %d FAILURE(S)\n", g_failures);
    return 1;
}
