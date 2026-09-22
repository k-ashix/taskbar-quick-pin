// Unit tests for the taskbar-layout decision math (refinement Issue 2:
// left-aligned taskbar leaves the mod stuck in STATE_BOOT).
//
// The production bug: RefreshTaskbarCache bailed with
//     if (newW <= 0 || startLeft <= tbr.left + 50) return;
// On a left-aligned taskbar Start sits at ~tbr.left + 16, so that condition is
// TRUE every call and the function returned before the state machine ran ->
// g_systemState never left STATE_BOOT, the dock was clamped over Start, and the
// worker polled at 100 ms forever.
//
// DecideTaskbarLayout replaces that single early-return with three explicit
// outcomes (OK / UNSUPPORTED / PENDING) so the caller can always drive the
// state machine to a terminal, low-cadence state. These tests pin its contract
// before the production code is wired up.
//
// Realistic constants mirrored from the mod:
//   MIN_VALID_DOCK_WIDTH = 80   (minRoom)
//   DOCK_GAP_PX          ~ 6    (dockGapPx)
//   dockWidth            ~ 200  (a typical fitted dock)

#include "taskbar_layout.h"
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

// ---- Centered taskbar (the default that always "worked") -------------------

static void test_centered_win11_primary_monitor() {
    // 1920-wide primary taskbar, Start centered so its left edge is ~880.
    // Plenty of room to the left -> OK.
    CHECK(DecideTaskbarLayout(/*tbrLeft*/0, /*tbrRight*/1920,
                              /*startLeft*/880, GAP, DOCKW, MINROOM) == LAYOUT_OK);
}

static void test_centered_second_monitor_offset() {
    // Multi-monitor: a secondary taskbar whose rect starts at x=1920.
    // Start centered at ~2800. room = (2800-6) - 1920 = 874 -> OK.
    CHECK(DecideTaskbarLayout(/*tbrLeft*/1920, /*tbrRight*/3840,
                              /*startLeft*/2800, GAP, DOCKW, MINROOM) == LAYOUT_OK);
}

// ---- Left-aligned taskbar (the bug) ----------------------------------------

static void test_left_aligned_win11_is_unsupported() {
    // Win11 "Align: Left": Start hugs the left edge (~tbr.left + 16).
    // room = (16-6) - 0 = 10 < 80 -> UNSUPPORTED (must NOT be PENDING: the
    // geometry is perfectly valid, there simply is no room).
    CHECK(DecideTaskbarLayout(/*tbrLeft*/0, /*tbrRight*/1920,
                              /*startLeft*/16, GAP, DOCKW, MINROOM) == LAYOUT_UNSUPPORTED);
}

static void test_win10_style_left_is_unsupported() {
    // Win10 taskbar: Start at the very left (~tbr.left + 12).
    CHECK(DecideTaskbarLayout(/*tbrLeft*/0, /*tbrRight*/1366,
                              /*startLeft*/12, GAP, DOCKW, MINROOM) == LAYOUT_UNSUPPORTED);
}

static void test_left_aligned_second_monitor_is_unsupported() {
    // Left-aligned on an offset monitor: Start at tbrLeft + 16 = 1936.
    // room = (1936-6) - 1920 = 10 < 80 -> UNSUPPORTED (regression guard: the
    // decision must use tbrLeft, not assume the taskbar starts at x=0).
    CHECK(DecideTaskbarLayout(/*tbrLeft*/1920, /*tbrRight*/3840,
                              /*startLeft*/1936, GAP, DOCKW, MINROOM) == LAYOUT_UNSUPPORTED);
}

// ---- Boundary between OK and UNSUPPORTED (off-by-one hunt) ------------------

static void test_room_exactly_equals_need_is_ok() {
    // need = min(dockWidth, minRoom) = min(200,80) = 80.
    // Choose startLeft so room == 80 exactly: room = (startLeft-GAP)-tbrLeft.
    // 80 = (startLeft - 6) - 0  ->  startLeft = 86.
    CHECK(DecideTaskbarLayout(0, 1920, 86, GAP, DOCKW, MINROOM) == LAYOUT_OK);
}

static void test_room_one_less_than_need_is_unsupported() {
    // startLeft = 85 -> room = 79 < 80 -> UNSUPPORTED. Proves the >= boundary.
    CHECK(DecideTaskbarLayout(0, 1920, 85, GAP, DOCKW, MINROOM) == LAYOUT_UNSUPPORTED);
}

static void test_small_dock_needs_only_its_own_width() {
    // A tiny dock (width 30) only needs 30 px, not the 80 px minimum, to fit.
    // room = (40-6)-0 = 34 >= 30 -> OK, even though 34 < 80.
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/40, GAP, /*dockWidth*/30, MINROOM) == LAYOUT_OK);
    // But the same 34 px of room is NOT enough for a 200 px dock -> its need is
    // capped at minRoom(80), so 34 < 80 -> UNSUPPORTED.
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/40, GAP, /*dockWidth*/200, MINROOM) == LAYOUT_UNSUPPORTED);
}

// ---- PENDING: geometry not trustworthy yet (must keep booting) -------------

static void test_degenerate_taskbar_rect_is_pending() {
    // Zero-width or inverted taskbar rect -> can't decide yet.
    CHECK(DecideTaskbarLayout(/*tbrLeft*/0, /*tbrRight*/0, 16, GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
    CHECK(DecideTaskbarLayout(/*tbrLeft*/100, /*tbrRight*/50, 120, GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
}

static void test_start_at_or_left_of_taskbar_left_is_pending() {
    // startLeft == tbrLeft or < tbrLeft is not a trustworthy detection ->
    // PENDING (keep retrying), NOT UNSUPPORTED. This is the key distinction: a
    // failed Start probe during cold boot must not permanently disable the dock.
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/0,  GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/-5, GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
}

static void test_start_at_or_right_of_taskbar_right_is_pending() {
    // A Start edge at/beyond the right edge is nonsensical -> PENDING.
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/1920, GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/5000, GAP, DOCKW, MINROOM) == LAYOUT_PENDING);
}

// ---- Robustness of odd-but-valid inputs ------------------------------------

static void test_negative_gap_is_treated_as_zero() {
    // A negative gap must not inflate the room. startLeft=80, gap=-100 would
    // give room=180 if gap leaked in; clamped to 0 it gives room=80 -> OK,
    // and importantly stays deterministic.
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/86, /*gap*/-100, DOCKW, MINROOM) == LAYOUT_OK);
}

static void test_zero_minroom_still_needs_one_pixel() {
    // Defensive: minRoom 0 must not make everything OK with zero room.
    // startLeft=6 -> room = (6-6)-0 = 0. need clamps up to 1 -> 0 < 1 -> UNSUPPORTED.
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/6, GAP, DOCKW, /*minRoom*/0) == LAYOUT_UNSUPPORTED);
    // startLeft=8 -> room = 2 >= 1 -> OK.
    CHECK(DecideTaskbarLayout(0, 1920, /*startLeft*/8, GAP, DOCKW, /*minRoom*/0) == LAYOUT_OK);
}

int main() {
    test_centered_win11_primary_monitor();
    test_centered_second_monitor_offset();
    test_left_aligned_win11_is_unsupported();
    test_win10_style_left_is_unsupported();
    test_left_aligned_second_monitor_is_unsupported();
    test_room_exactly_equals_need_is_ok();
    test_room_one_less_than_need_is_unsupported();
    test_small_dock_needs_only_its_own_width();
    test_degenerate_taskbar_rect_is_pending();
    test_start_at_or_left_of_taskbar_left_is_pending();
    test_start_at_or_right_of_taskbar_right_is_pending();
    test_negative_gap_is_treated_as_zero();
    test_zero_minroom_still_needs_one_pixel();

    if (g_failures == 0) {
        std::printf("ALL PASS\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
