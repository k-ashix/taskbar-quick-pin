// Unit tests for the keyboard vs rapid-click gesture gating (the "gestures do
// nothing even when enabled" bug). The keyboard P/U/L gestures act on the
// foreground window and must be evaluated whenever the setting is on, WITHOUT
// requiring the dock's overlay window -- that dependency was the bug. The
// rapid-click unpin-all gesture hit-tests dock icons, so it correctly still
// requires the overlay window.

#include "key_gesture_gating.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_key_gesture_off_by_default() {
    CHECK(ShouldEvalKeyGesture(/*enabled*/false) == false);
}

static void test_key_gesture_runs_when_enabled() {
    CHECK(ShouldEvalKeyGesture(/*enabled*/true) == true);
}

static void test_key_gesture_independent_of_overlay() {
    // The regression: with the setting ON, the keys must fire regardless of
    // whether the overlay window exists. ShouldEvalKeyGesture takes no overlay
    // argument at all -- enabling the feature is the only condition.
    CHECK(ShouldEvalKeyGesture(true) == true);   // (no overlay-state input by design)
}

static void test_rapid_click_needs_overlay() {
    // Rapid-click unpin-all hit-tests icons, so it needs the dock window even
    // when enabled.
    CHECK(ShouldEvalRapidClick(/*enabled*/true,  /*overlay*/true)  == true);
    CHECK(ShouldEvalRapidClick(/*enabled*/true,  /*overlay*/false) == false);
    CHECK(ShouldEvalRapidClick(/*enabled*/false, /*overlay*/true)  == false);
}

int main() {
    test_key_gesture_off_by_default();
    test_key_gesture_runs_when_enabled();
    test_key_gesture_independent_of_overlay();
    test_rapid_click_needs_overlay();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
