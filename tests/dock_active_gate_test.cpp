// Unit tests for the dock-inactive gate (v2.5.3): a hidden/suppressed dock must
// be a NON-INTERACTIVE dock -- no drag / resolve / dock-zone / pin-unpin / glow
// work may run against stale cached geometry while the overlay is not on screen.
//
// Mirrors DockInteractionAllowed in the mod's WorkerThread drag-state gate.

#include "dock_active_gate.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// A fully healthy, on-screen dock: every suppression flag clear, real geometry.
static void test_healthy_dock_is_interactive() {
    CHECK(DockInteractionAllowed(/*fullscreen=*/false, /*unsupported=*/false,
                                 /*haveTaskbar=*/true, /*dockW=*/240,
                                 /*rectValid=*/true, /*autoHideHidden=*/false) == true);
}

static void test_fullscreen_disables_interaction() {
    CHECK(DockInteractionAllowed(true, false, true, 240, true, false) == false);
}

static void test_unsupported_layout_disables_interaction() {
    CHECK(DockInteractionAllowed(false, true, true, 240, true, false) == false);
}

static void test_missing_taskbar_disables_interaction() {
    // Explorer restarting: no cached taskbar -> nothing to interact with.
    CHECK(DockInteractionAllowed(false, false, false, 240, true, false) == false);
}

static void test_zero_width_disables_interaction() {
    // Geometry not resolved yet (boot race) or torn down: no live dock.
    CHECK(DockInteractionAllowed(false, false, true, 0, true, false) == false);
    CHECK(DockInteractionAllowed(false, false, true, -1, true, false) == false);
}

static void test_invalid_cached_rect_disables_interaction() {
    // Cached dock rectangle is empty/invalid -> no drop zone to hit-test.
    CHECK(DockInteractionAllowed(false, false, true, 240, false, false) == false);
}

// Issue 3 (auto-hide): with "Sync with taskbar auto-hide" ON, RepositionOverlay
// hides the dock/input window when the taskbar has slid off screen. The cached
// dock rect stays valid, so WITHOUT this gate the drag pipeline would keep
// resolving / hit-testing / pinning against a dock the user cannot see. A dock
// hidden by auto-hide must be non-interactive.
static void test_autohide_hidden_disables_interaction() {
    CHECK(DockInteractionAllowed(false, false, true, 240, true,
                                 /*autoHideHidden=*/true) == false);
}

// Auto-hide ON but the taskbar (and dock) are on screen -> still interactive.
static void test_autohide_visible_stays_interactive() {
    CHECK(DockInteractionAllowed(false, false, true, 240, true,
                                 /*autoHideHidden=*/false) == true);
}

static void test_any_single_suppressor_wins() {
    // Fail-closed: if ANY suppressor is set the dock is non-interactive, even if
    // the others look fine.
    CHECK(DockInteractionAllowed(true,  false, true,  240, true,  false) == false);
    CHECK(DockInteractionAllowed(false, true,  true,  240, true,  false) == false);
    CHECK(DockInteractionAllowed(false, false, false, 240, true,  false) == false);
    CHECK(DockInteractionAllowed(false, false, true,  0,   true,  false) == false);
    CHECK(DockInteractionAllowed(false, false, true,  240, false, false) == false);
    CHECK(DockInteractionAllowed(false, false, true,  240, true,  true)  == false);
}

int main() {
    test_healthy_dock_is_interactive();
    test_fullscreen_disables_interaction();
    test_unsupported_layout_disables_interaction();
    test_missing_taskbar_disables_interaction();
    test_zero_width_disables_interaction();
    test_invalid_cached_rect_disables_interaction();
    test_autohide_hidden_disables_interaction();
    test_autohide_visible_stays_interactive();
    test_any_single_suppressor_wins();

    if (g_failures == 0) {
        std::printf("dock_active_gate: ALL PASS\n");
        return 0;
    }
    std::printf("dock_active_gate: %d FAILURE(S)\n", g_failures);
    return 1;
}
