// Unit tests for Issue 3: the dock windows must never be shown before there is
// a valid, owned dock. Visibility is gated on ownership + supported layout +
// real geometry, and nothing is shown from the UI thread.

#include "overlay_visibility_gate.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static OverlayGateInput make(bool owned, bool unsupported, int w, bool tb) {
    OverlayGateInput in;
    in.ownershipDecided  = owned;
    in.layoutUnsupported = unsupported;
    in.dockLocalW        = w;
    in.taskbarPresent    = tb;
    return in;
}

// ---- The one case that may show: owned, supported, real geometry ------------

static void test_shows_only_when_owned_supported_and_sized() {
    CHECK(ShouldShowDock(make(/*owned*/true, /*unsupported*/false,
                              /*w*/200, /*tb*/true)));
}

// ---- The top-left-corner bug: unsupported layout must stay hidden -----------

static void test_unsupported_layout_stays_hidden_even_when_owned() {
    // Windows 10 / left-aligned Windows 11: owned, but UNSUPPORTED. The 1/255
    // input window must NOT be shown in the top-left corner.
    CHECK(!ShouldShowDock(make(true, /*unsupported*/true, 200, true)));
    // ...even if a stale geometry width is lying around.
    CHECK(!ShouldShowDock(make(true, true, 200, true)));
}

// ---- DEFER / DISOWN: ownership not decided -> hidden ------------------------

static void test_not_owned_never_shows() {
    // Cold start (DEFER) before the worker resolves ownership.
    CHECK(!ShouldShowDock(make(/*owned*/false, false, 200, true)));
    // A non-owner explorer.exe (DISOWN) never sets ownershipDecided here.
    CHECK(!ShouldShowDock(make(false, false, 0, true)));
    CHECK(!ShouldShowDock(make(false, true, 0, false)));
}

// ---- Cold-start flash: no taskbar -> no synthesized geometry -> hidden ------

static void test_no_taskbar_fallback_returns_hidden() {
    // Owned + supported, but Shell_TrayWnd not up yet and no real width. The
    // fallback must NOT force a 200px dock at (0,0); the window stays hidden.
    OverlayGateInput noTb = make(/*owned*/true, /*unsupported*/false,
                                 /*w*/0, /*tb*/false);
    CHECK(ResolvedDockWidth(noTb) == 0);
    CHECK(!ShouldShowDock(noTb));

    // With a taskbar present, the fallback may synthesize 200 and then show.
    OverlayGateInput withTb = make(true, false, /*w*/0, /*tb*/true);
    CHECK(ResolvedDockWidth(withTb) == 200);
    CHECK(ShouldShowDock(withTb));
}

// ---- Real geometry is preferred over the fallback ---------------------------

static void test_real_width_is_used_when_present() {
    CHECK(ResolvedDockWidth(make(true, false, 173, true)) == 173);
    CHECK(ResolvedDockWidth(make(true, false, 173, false)) == 173);
}

// ---- The UI thread must never show anything ---------------------------------

static void test_ui_thread_never_shows_windows() {
    // "show first, decide later" is exactly the bug -- the UI thread creates the
    // windows but must leave them hidden for the worker to reveal.
    CHECK(!UiThreadShowsWindows());
}

int main() {
    test_shows_only_when_owned_supported_and_sized();
    test_unsupported_layout_stays_hidden_even_when_owned();
    test_not_owned_never_shows();
    test_no_taskbar_fallback_returns_hidden();
    test_real_width_is_used_when_present();
    test_ui_thread_never_shows_windows();

    if (g_failures == 0) {
        std::printf("overlay_visibility_gate: ALL PASS\n");
        return 0;
    }
    std::printf("overlay_visibility_gate: %d FAILURE(S)\n", g_failures);
    return 1;
}
