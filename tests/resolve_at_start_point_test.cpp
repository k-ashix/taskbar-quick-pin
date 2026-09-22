// Unit tests for ResolvePoint (Stage 5 - resolve at the recorded start point).
//
// The Prove-It test for the bug fixed this pass: resolving at the live cursor
// (which is over the dock/taskbar when dock intent fires) resolved the wrong
// window. Stage 5 must resolve at g_dragStartPt instead.

#include "resolve_at_start_point.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_resolves_at_start_point_not_live_cursor() {
    // Grabbed an Explorer window at (1200, 500). Dragged to the dock; the
    // cursor is now over a taskbar button at (40, 1050) when dock intent fires.
    TqpPoint start = {1200, 500};
    TqpPoint live  = {40, 1050};

    TqpPoint used = ResolvePoint(start, live);

    CHECK(PointsEqual(used, start) == true);   // resolves the grabbed window
    CHECK(PointsEqual(used, live)  == false);  // NOT the hovered taskbar button
}

static void test_live_cursor_never_changes_result() {
    // Same start point, wildly different live cursors -> identical resolve point.
    TqpPoint start = {777, 333};
    TqpPoint a = ResolvePoint(start, TqpPoint{0, 0});
    TqpPoint b = ResolvePoint(start, TqpPoint{1919, 1079});
    TqpPoint c = ResolvePoint(start, TqpPoint{-500, 42});

    CHECK(PointsEqual(a, start) == true);
    CHECK(PointsEqual(b, start) == true);
    CHECK(PointsEqual(c, start) == true);
    CHECK(PointsEqual(a, b) == true);
    CHECK(PointsEqual(b, c) == true);
}

static void test_degenerate_case_start_equals_cursor() {
    // If they happen to coincide, the recorded point is still what's used.
    TqpPoint p = {10, 20};
    CHECK(PointsEqual(ResolvePoint(p, p), p) == true);
}

int main() {
    test_resolves_at_start_point_not_live_cursor();
    test_live_cursor_never_changes_result();
    test_degenerate_case_start_equals_cursor();

    if (g_failures == 0) {
        std::printf("resolve_at_start_point: ALL PASS\n");
        return 0;
    }
    std::printf("resolve_at_start_point: %d FAILURE(S)\n", g_failures);
    return 1;
}
