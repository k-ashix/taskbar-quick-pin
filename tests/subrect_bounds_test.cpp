// Unit tests for the content sub-rect bounds math used by the rope/vanish
// layered-surface present path (refinement item 8).
//
// Item 8 problem: PresentPhysicsRope and UpdateVanishWindow memset + premultiply
// their ENTIRE fixed DIB every frame (up to 810x810 for the rope, 1600x512 for
// the vanish surface) even though only a small content sub-rect is ever drawn
// into. This is ~320 MB/s of memset plus tens of millions of premultiply ops per
// second inside explorer.exe for a decorative effect.
//
// The fix clears + premultiplies only the content sub-rect. Because the layered
// window is fixed-size and only MOVES (never resizes -- that is what avoids the
// black-slab flash on real GPUs), and UpdateLayeredWindow still presents the
// whole surface each frame, the cleared rect MUST cover both this frame's
// content AND the previous frame's content, otherwise pixels drawn last frame
// linger as garbage when the effect shrinks.
//
// ComputeClearBounds is the pure geometry that decides which rows/cols to touch.
// These tests pin its contract before the production code is wired up.

#include "subrect_bounds.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Content is always anchored at the buffer top-left (drawn at coord-left /
// mote-origin, with the margin folded into left/top), so a content extent of
// (cw, ch) occupies buffer rect [0,cw) x [0,ch).

static void test_covers_current_content() {
    // Fresh surface (no previous frame): clear exactly the current content box.
    ClearBounds b = ComputeClearBounds(/*cw*/100, /*ch*/40,
                                       /*prevW*/0, /*prevH*/0,
                                       /*surfW*/800, /*surfH*/800);
    CHECK(b.x == 0 && b.y == 0);
    CHECK(b.w == 100);
    CHECK(b.h == 40);
}

static void test_covers_previous_when_shrinking() {
    // Previous frame drew a 300x120 box; this frame only needs 100x40.
    // We must still clear the old 300x120 so last frame's pixels don't linger.
    ClearBounds b = ComputeClearBounds(/*cw*/100, /*ch*/40,
                                       /*prevW*/300, /*prevH*/120,
                                       /*surfW*/800, /*surfH*/800);
    CHECK(b.w == 300);
    CHECK(b.h == 120);
}

static void test_covers_current_when_growing() {
    // This frame is bigger than the last -> clear the current (larger) box.
    ClearBounds b = ComputeClearBounds(/*cw*/500, /*ch*/200,
                                       /*prevW*/100, /*prevH*/40,
                                       /*surfW*/800, /*surfH*/800);
    CHECK(b.w == 500);
    CHECK(b.h == 200);
}

static void test_clamped_to_surface() {
    // Content extent must never exceed the fixed surface (guards OOB writes).
    ClearBounds b = ComputeClearBounds(/*cw*/2000, /*ch*/900,
                                       /*prevW*/0, /*prevH*/0,
                                       /*surfW*/1600, /*surfH*/512);
    CHECK(b.w == 1600);
    CHECK(b.h == 512);
}

static void test_negative_and_zero_are_safe() {
    // Degenerate inputs must not produce negative spans (no UB in memset/loops).
    ClearBounds b = ComputeClearBounds(/*cw*/-5, /*ch*/0,
                                       /*prevW*/-1, /*prevH*/-1,
                                       /*surfW*/800, /*surfH*/800);
    CHECK(b.w == 0);
    CHECK(b.h == 0);
}

static void test_much_smaller_than_surface_saves_work() {
    // The whole point: a tiny rope on a huge surface clears a tiny area.
    ClearBounds b = ComputeClearBounds(/*cw*/60, /*ch*/60,
                                       /*prevW*/60, /*prevH*/60,
                                       /*surfW*/810, /*surfH*/810);
    long long cleared = (long long)b.w * b.h;
    long long full    = 810LL * 810LL;
    CHECK(cleared * 50 < full);   // >50x less pixel work than the whole surface
}

int main() {
    test_covers_current_content();
    test_covers_previous_when_shrinking();
    test_covers_current_when_growing();
    test_clamped_to_surface();
    test_negative_and_zero_are_safe();
    test_much_smaller_than_surface_saves_work();

    if (g_failures == 0) {
        std::printf("ALL PASS\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
