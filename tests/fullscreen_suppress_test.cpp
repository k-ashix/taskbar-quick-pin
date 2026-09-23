// Unit tests for the pure fullscreen-suppression decision (fullscreen_suppress.h).
//
// Pins WHEN the whole dock hides (fullscreen app / exclusive presentation /
// secure snip overlay) and the anti-flicker hysteresis that keeps enter/exit
// from feeling like a hide-then-reappear blink. Self-contained (own main());
// compiled independently by run_tests.ps1.

#include "fullscreen_suppress.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_normal_desktop_never_suppresses() {
    // Nothing fullscreen: a normal windowed foreground, or the desktop/shell.
    CHECK(!ShouldSuppressForFullscreen(false, false, false));   // small window
    CHECK(!ShouldSuppressForFullscreen(false, true,  false));   // desktop/shell focused
}

static void test_fullscreen_app_suppresses() {
    // A borderless / F11 fullscreen app that covers its whole monitor and is
    // not the shell -> hide the dock (matches taskbar auto-hide behaviour).
    CHECK(ShouldSuppressForFullscreen(true, false, false));
}

static void test_shell_covering_monitor_is_not_fullscreen() {
    // The shell/desktop legitimately covers the whole monitor; that must NOT
    // count as a fullscreen app, or the dock would hide on a plain desktop.
    CHECK(!ShouldSuppressForFullscreen(true, true, false));
}

static void test_exclusive_presentation_suppresses() {
    // SHQueryUserNotificationState reported exclusive D3D / presentation mode
    // (fullscreen game, PowerPoint show) -> hide regardless of the rect check.
    CHECK(ShouldSuppressForFullscreen(false, false, true));
    CHECK(ShouldSuppressForFullscreen(false, true,  true));
}

static void test_hysteresis_hides_immediately() {
    // The instant fullscreen is detected, hide -- never paint one dock frame
    // over a game or the snip overlay.
    CHECK(FullscreenHideDecision(/*suppressNow=*/true, /*prevActive=*/false, /*missCount=*/0, /*restoreDelayFrames=*/2));
    CHECK(FullscreenHideDecision(true, true, 99, 2));   // still hidden even after many clears if detected again
}

static void test_hysteresis_delays_restore() {
    const int delay = 2;
    // The dock WAS hidden for a fullscreen app (prevActive=true) and is now
    // clearing. First clear sample: still hidden (miss=0 < delay).
    CHECK(FullscreenHideDecision(false, true, 0, delay));
    // Second consecutive clear sample: still hidden (miss=1 < delay).
    CHECK(FullscreenHideDecision(false, true, 1, delay));
    // Enough consecutive clear samples: restore (show) the dock.
    CHECK(!FullscreenHideDecision(false, true, 2, delay));
    CHECK(!FullscreenHideDecision(false, true, 5, delay));
}

static void test_never_manufactures_hide_when_not_already_hidden() {
    // REGRESSION (boot / Start-menu / Search reveal blink): when the dock is
    // NOT already hidden (prevActive=false) and nothing is fullscreen, the
    // restore hysteresis must NOT invent a hide -- even though missCount starts
    // at 0. Before the fix the 3-arg FullscreenHideDecision(false, 0, 2)
    // returned true, so at boot (g_fullscreenActive=false, g_fullscreenMiss=0)
    // the dock hid for ~2 polls then re-revealed: the visible blink the user
    // saw when opening Start/Search and on every mod init.
    CHECK(!FullscreenHideDecision(false, false, 0, 2));
    CHECK(!FullscreenHideDecision(false, false, 1, 2));
    CHECK(!FullscreenHideDecision(false, false, 99, 2));
}

static void test_transition_edges_for_logging() {
    // Edge classifier so the mod logs exactly ONE line when the dock starts
    // hiding and one when it is restored -- never a per-frame stream.
    CHECK(FullscreenTransitionEvent(false, false) == FS_NONE);      // idle steady state
    CHECK(FullscreenTransitionEvent(true,  true ) == FS_NONE);      // still hidden, no new log
    CHECK(FullscreenTransitionEvent(false, true ) == FS_ENTERED);   // dock just hid
    CHECK(FullscreenTransitionEvent(true,  false) == FS_CLEARED);   // dock about to restore
}

int main() {
    test_normal_desktop_never_suppresses();
    test_fullscreen_app_suppresses();
    test_shell_covering_monitor_is_not_fullscreen();
    test_exclusive_presentation_suppresses();
    test_hysteresis_hides_immediately();
    test_hysteresis_delays_restore();
    test_never_manufactures_hide_when_not_already_hidden();
    test_transition_edges_for_logging();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
