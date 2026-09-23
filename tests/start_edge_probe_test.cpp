// Unit tests for the Start-button probe -> layout decision (roadmap Issue 1B:
// GetStartButtonLeftEdge must not fabricate a 1/5-width edge when Start is not
// found; a not-found probe must stay PENDING).
//
// The production bug: GetStartButtonLeftEdge returned
//     tbRect.left + (tbRect.right - tbRect.left) / 5
// on a failed probe, so QpDecideTaskbarLayout committed to a real layout for a
// Start position that was never resolved. DecideLayoutFromStartProbe gates the
// layout on an explicit found/not-found result, so these tests pin that a
// not-found probe is always PENDING and never adopts the old estimate.
//
// Realistic constants mirrored from the mod:
//   MIN_VALID_DOCK_WIDTH = 80   (minRoom)
//   DOCK_GAP_PX          ~ 6    (gap)
//   dockWidth            ~ 200  (a typical fitted dock)

#include "start_edge_probe.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static const int MINROOM = 80;   // MIN_VALID_DOCK_WIDTH
static const int GAP     = 6;    // DOCK_GAP_PX
static const int DOCKW   = 200;  // typical fitted dock width

// ---- The fix: a NOT-FOUND probe is always PENDING, never the 1/5 estimate ---

static void test_not_found_is_pending_even_where_the_fake_edge_would_be_ok() {
    // 1920-wide taskbar. The OLD fabricated edge would be
    //   tbrLeft + (tbrRight-tbrLeft)/5 = 0 + 1920/5 = 384,
    // and DecideTaskbarLayout(0,1920,384,...) has huge room -> LAYOUT_OK.
    // With the fix, a not-found probe short-circuits to PENDING regardless.
    const long fakeEdge = 0 + (1920 - 0) / 5;  // == 384, the old estimate
    CHECK(DecideTaskbarLayout(0, 1920, fakeEdge, GAP, DOCKW, MINROOM) == LAYOUT_OK);  // old path WAS OK
    CHECK(DecideLayoutFromStartProbe(/*startFound*/false, /*measured*/0,
                                     0, 1920, GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
}

static void test_not_found_on_offset_monitor_is_pending() {
    CHECK(DecideLayoutFromStartProbe(/*startFound*/false, /*measured*/0,
                                     1920, 3840, GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
}

// ---- A FOUND probe defers to DecideTaskbarLayout (no over-rejection) --------

static void test_found_centered_is_ok() {
    CHECK(DecideLayoutFromStartProbe(/*startFound*/true, /*measured*/880,
                                     0, 1920, GAP, DOCKW, MINROOM) == LAYOUT_OK);
}

static void test_found_left_aligned_is_unsupported_not_pending() {
    // Regression guard: a genuinely left-aligned Start (found, hugs the edge)
    // must reach UNSUPPORTED via DecideTaskbarLayout, NOT be swallowed as a
    // not-found PENDING. Start at tbrLeft+16.
    CHECK(DecideLayoutFromStartProbe(/*startFound*/true, /*measured*/16,
                                     0, 1920, GAP, DOCKW, MINROOM) == LAYOUT_UNSUPPORTED);
}

static void test_found_left_aligned_offset_monitor_is_unsupported() {
    CHECK(DecideLayoutFromStartProbe(/*startFound*/true, /*measured*/1936,
                                     1920, 3840, GAP, DOCKW, MINROOM) == LAYOUT_UNSUPPORTED);
}

static void test_found_but_nonsensical_edge_still_pending() {
    // Found, but the measured edge is at/right of tbrRight (an impossible read):
    // DecideTaskbarLayout returns PENDING and we surface that unchanged.
    CHECK(DecideLayoutFromStartProbe(/*startFound*/true, /*measured*/2000,
                                     0, 1920, GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
}

int main() {
    test_not_found_is_pending_even_where_the_fake_edge_would_be_ok();
    test_not_found_on_offset_monitor_is_pending();
    test_found_centered_is_ok();
    test_found_left_aligned_is_unsupported_not_pending();
    test_found_left_aligned_offset_monitor_is_unsupported();
    test_found_but_nonsensical_edge_still_pending();

    if (g_failures == 0) {
        std::printf("start_edge_probe: ALL PASS\n");
        return 0;
    }
    std::printf("start_edge_probe: %d FAILURE(S)\n", g_failures);
    return 1;
}
