// Unit tests for the pure lock/unlock glow decision logic (tests/lock_glow.h).
//
// The actual on-screen bloom is a Win32 layered-window effect that can only be
// verified by eye on a live Windhawk desktop; here we pin the pure math that
// drives it: kind selection from the lock state, the flash alpha envelope, the
// gold-vs-green colour ramp, and the seal-converges / release-expands geometry
// direction. Self-contained (own main()); compiled independently by run_tests.ps1.

#include "lock_glow.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_kind_from_locked() {
    // Trigger reads the ALREADY-updated lock flag: locked => gold seal-in,
    // unlocked => green release. This is how lock/unlock stay distinct without
    // touching any keyboard-gesture call site.
    CHECK(LockGlowKindFromLocked(true)  == LOCKGLOW_SEAL);
    CHECK(LockGlowKindFromLocked(false) == LOCKGLOW_RELEASE);
}

static void test_alpha_envelope() {
    CHECK(LockGlowAlpha01(0.0f) == 0.0f);   // fully transparent at start
    CHECK(LockGlowAlpha01(1.0f) == 0.0f);   // fully transparent at end
    CHECK(LockGlowAlpha01(0.22f) > 0.5f);   // strong peak
    // Peak sits early in the flash: an early sample beats a late one.
    CHECK(LockGlowAlpha01(0.22f) > LockGlowAlpha01(0.80f));
    // Monotonic rise from near-zero to the peak.
    CHECK(LockGlowAlpha01(0.10f) > LockGlowAlpha01(0.02f));
    // Fades out after the peak.
    CHECK(LockGlowAlpha01(0.50f) > LockGlowAlpha01(0.90f));
}

static void test_color_ramp_gold_vs_green() {
    LockGlowRGB gold = LockGlowColor(LOCKGLOW_SEAL, 1.0f);
    CHECK(gold.r > gold.b);     // gold is warm: red high vs blue
    CHECK(gold.g > gold.b);     // gold: green high vs blue
    CHECK(gold.r >= gold.g);    // leans amber/gold (red >= green)

    LockGlowRGB green = LockGlowColor(LOCKGLOW_RELEASE, 1.0f);
    CHECK(green.g > green.r);   // green channel dominant
    CHECK(green.g > green.b);

    // Both ramps brighten toward the peak.
    CHECK(LockGlowColor(LOCKGLOW_SEAL, 1.0f).r    > LockGlowColor(LOCKGLOW_SEAL, 0.0f).r);
    CHECK(LockGlowColor(LOCKGLOW_RELEASE, 1.0f).g > LockGlowColor(LOCKGLOW_RELEASE, 0.0f).g);

    // LIMIT (dock-full / pin-limit feedback) is a RED ramp: red channel dominant.
    LockGlowRGB red = LockGlowColor(LOCKGLOW_LIMIT, 1.0f);
    CHECK(red.r > red.g);   // red dominant vs green
    CHECK(red.r > red.b);   // red dominant vs blue
    CHECK(LockGlowColor(LOCKGLOW_LIMIT, 1.0f).r > LockGlowColor(LOCKGLOW_LIMIT, 0.0f).r);  // brightens toward peak
}

static void test_rounded_corners_clip() {
    // Dock 100x40 with a big corner radius. A pixel at the extreme top-left
    // CORNER is outside the rounded rect -> not lit; the same-x pixel at the
    // vertical MIDDLE (a straight edge) is inside -> lit. Proves the glow
    // follows the dock's real rounded corners rather than a hard box.
    const float dl = 0, dt = 0, dr = 100, db = 40;
    const float R = 18.0f;                 // corner radius
    // Sharp corner pixel (1,1) sits well outside an 18px rounded corner.
    CHECK(LockGlowRoundRectSD(1.0f, 1.0f, dl, dt, dr, db, R) > 0.0f);
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_SWEEP, 1.0f, 1.0f, dl, dt, dr, db, 1.0f, 4.0f, R));
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE,  1.0f, 1.0f, dl, dt, dr, db, 1.0f, 4.0f, R));
    // Straight left edge at mid-height (x=1, y=20) is inside -> lit in EDGE mode.
    CHECK(LockGlowRoundRectSD(1.0f, 20.0f, dl, dt, dr, db, R) < 0.0f);
    CHECK(LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE, 1.0f, 20.0f, dl, dt, dr, db, 1.0f, 4.0f, R));
    // A square rect (R=0) still lights that same corner pixel: rounding is
    // strictly opt-in via cornerR.
    CHECK(LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE, 1.0f, 1.0f, dl, dt, dr, db, 1.0f, 4.0f, 0.0f));
}

static void test_mode_from_setting() {
    // The user animation toggle is OFF by default -> edge-only highlight.
    CHECK(LockGlowModeFromSetting(false) == LOCKGLOW_MODE_EDGE);
    // Toggle ON -> full left-to-right sweep.
    CHECK(LockGlowModeFromSetting(true)  == LOCKGLOW_MODE_SWEEP);
}

static void test_sweep_left_to_right() {
    // Sweep front starts at the left dock edge and ends at the right edge.
    CHECK(LockGlowSweepX01(0.0f) == 0.0f);
    CHECK(LockGlowSweepX01(1.0f) == 1.0f);
    // Monotonic non-decreasing: the fill only ever advances rightward.
    CHECK(LockGlowSweepX01(0.25f) <= LockGlowSweepX01(0.50f));
    CHECK(LockGlowSweepX01(0.50f) <= LockGlowSweepX01(0.75f));
    // A real advance happens by the midpoint.
    CHECK(LockGlowSweepX01(0.50f) > 0.0f);
    CHECK(LockGlowSweepX01(0.50f) < 1.0f);
}

static void test_confined_to_dock() {
    // Dock interior box in buffer coords: [10,4) .. (110,40).
    const float dl = 10, dt = 4, dr = 110, db = 40;

    // Nothing outside the dock rectangle is ever allowed, in EITHER mode.
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_SWEEP, 5.0f,  20.0f, dl, dt, dr, db, 1.0f, 3.0f)); // left of dock
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_SWEEP, 200.0f,20.0f, dl, dt, dr, db, 1.0f, 3.0f)); // right of dock
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_SWEEP, 50.0f, 0.0f,  dl, dt, dr, db, 1.0f, 3.0f)); // above dock
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_SWEEP, 50.0f, 100.0f,dl, dt, dr, db, 1.0f, 3.0f)); // below dock
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE,  5.0f,  20.0f, dl, dt, dr, db, 1.0f, 3.0f)); // left of dock
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE,  200.0f,20.0f, dl, dt, dr, db, 1.0f, 3.0f)); // right of dock
}

static void test_sweep_front_gates_fill() {
    const float dl = 10, dt = 4, dr = 110, db = 40;   // width 100
    // Front at 25% => x = 10 + 25 = 35. Pixel at x=20 is behind the front (lit),
    // pixel at x=90 is ahead of the front (not yet lit).
    CHECK( LockGlowPixelAllowed(LOCKGLOW_MODE_SWEEP, 20.0f, 20.0f, dl, dt, dr, db, 0.25f, 3.0f));
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_SWEEP, 90.0f, 20.0f, dl, dt, dr, db, 0.25f, 3.0f));
    // Fully swept (front=1): the whole interior is lit.
    CHECK( LockGlowPixelAllowed(LOCKGLOW_MODE_SWEEP, 90.0f, 20.0f, dl, dt, dr, db, 1.0f, 3.0f));
}

static void test_edge_band_is_inner_only() {
    const float dl = 10, dt = 4, dr = 110, db = 40;
    const float band = 3.0f;
    // A pixel right on the inner left edge is within the band -> lit.
    CHECK( LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE, 11.0f, 20.0f, dl, dt, dr, db, 1.0f, band));
    // A pixel deep in the dock centre is NOT near any edge -> not lit
    // (edge mode never fills the middle; that is the sweep's job).
    CHECK(!LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE, 60.0f, 22.0f, dl, dt, dr, db, 1.0f, band));
    // Near the inner right/top/bottom edges -> lit.
    CHECK( LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE, 108.0f, 20.0f, dl, dt, dr, db, 1.0f, band));
    CHECK( LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE, 60.0f,  5.0f,  dl, dt, dr, db, 1.0f, band));
    CHECK( LockGlowPixelAllowed(LOCKGLOW_MODE_EDGE, 60.0f,  38.0f, dl, dt, dr, db, 1.0f, band));
}

static void test_peak_alpha_is_calmer() {
    // The softened ceiling keeps the flash gentle (well under a full-opacity blink).
    CHECK(LOCKGLOW_PEAK_ALPHA > 0.0f);
    CHECK(LOCKGLOW_PEAK_ALPHA < 0.8f);
}

static void test_dock_hidden_for_glow() {
    // The glow rides the dock's real rectangle, so it must be considered hidden
    // whenever the dock overlay itself is hidden -- for ANY of the three hard
    // hide reasons in RepositionOverlay.
    CHECK(!DockHiddenForGlow(false, false, 200));  // normal, visible dock
    CHECK( DockHiddenForGlow(true,  false, 200));  // fullscreen app / snip
    CHECK( DockHiddenForGlow(false, true,  200));  // unsupported (left-aligned) layout
    CHECK( DockHiddenForGlow(false, false, 0));    // zero-width boot race
    CHECK( DockHiddenForGlow(false, false, -5));   // degenerate negative width
}

static void test_lock_glow_teardown() {
    const unsigned dur = 750;
    // No flash in flight -> nothing to remove, whatever the dock is doing.
    CHECK(!LockGlowShouldTeardown(false, false, 0,    dur));
    CHECK(!LockGlowShouldTeardown(false, true,  9999, dur));
    // Flash active, dock visible, still within duration -> keep animating.
    CHECK(!LockGlowShouldTeardown(true,  false, 100,  dur));
    // Flash active but the dock got hidden -> remove NOW (the stuck-glow fix:
    // never leave the glow painting over empty space where the dock used to be).
    CHECK( LockGlowShouldTeardown(true,  true,  100,  dur));
    // Flash active and duration elapsed -> remove.
    CHECK( LockGlowShouldTeardown(true,  false, 750,  dur));
    CHECK( LockGlowShouldTeardown(true,  false, 800,  dur));
}

int main() {
    test_kind_from_locked();
    test_alpha_envelope();
    test_color_ramp_gold_vs_green();
    test_rounded_corners_clip();
    test_mode_from_setting();
    test_sweep_left_to_right();
    test_confined_to_dock();
    test_sweep_front_gates_fill();
    test_edge_band_is_inner_only();
    test_peak_alpha_is_calmer();
    test_dock_hidden_for_glow();
    test_lock_glow_teardown();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
