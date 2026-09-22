// Unit tests for the Layer-3 process-fallback accept logic (functionality note
// #1). The old resolver accepted any visible top-level window within 50 px of
// the cursor, so a press on the desktop or on a window's edge could resolve to
// a neighbouring app and pin the wrong thing. The contract these tests pin:
//
//   * Layer3PointRectDistance is a Manhattan gap: 0 inside, dx+dy outside.
//   * Layer3AcceptsDistance accepts ONLY dist == 0 (cursor inside the window).
//     A near-miss (edge, neighbour) must be rejected.

#include "layer3_accept.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// A representative window rect for the tests: [100,100)-(300,200).
static const int L = 100, T = 100, R = 300, B = 200;

static void test_distance_zero_when_inside() {
    CHECK(Layer3PointRectDistance(200, 150, L, T, R, B) == 0);
    CHECK(Layer3PointRectDistance(L,   T,   L, T, R, B) == 0);  // top-left corner
    CHECK(Layer3PointRectDistance(R,   B,   L, T, R, B) == 0);  // bottom-right corner
}

static void test_distance_horizontal_gap() {
    // 10 px to the left of the rect, vertically inside.
    CHECK(Layer3PointRectDistance(90, 150, L, T, R, B) == 10);
    // 5 px to the right.
    CHECK(Layer3PointRectDistance(305, 150, L, T, R, B) == 5);
}

static void test_distance_vertical_gap() {
    CHECK(Layer3PointRectDistance(200, 80,  L, T, R, B) == 20);  // 20 px above
    CHECK(Layer3PointRectDistance(200, 230, L, T, R, B) == 30);  // 30 px below
}

static void test_distance_diagonal_is_sum() {
    // 10 px left + 20 px above -> Manhattan sum 30.
    CHECK(Layer3PointRectDistance(90, 80, L, T, R, B) == 30);
}

static void test_accepts_only_inside() {
    CHECK(Layer3AcceptsDistance(0) == true);
}

static void test_rejects_near_miss() {
    // The exact regression: a 1 px edge miss, and anything up to the old 50 px
    // radius, must now be rejected.
    CHECK(Layer3AcceptsDistance(1)  == false);
    CHECK(Layer3AcceptsDistance(10) == false);
    CHECK(Layer3AcceptsDistance(50) == false);
}

static void test_accept_composed_with_distance() {
    // End to end: a cursor 10 px outside the window is rejected; inside accepted.
    CHECK(Layer3AcceptsDistance(Layer3PointRectDistance(90, 150, L, T, R, B)) == false);
    CHECK(Layer3AcceptsDistance(Layer3PointRectDistance(200, 150, L, T, R, B)) == true);
}

int main() {
    test_distance_zero_when_inside();
    test_distance_horizontal_gap();
    test_distance_vertical_gap();
    test_distance_diagonal_is_sum();
    test_accepts_only_inside();
    test_rejects_near_miss();
    test_accept_composed_with_distance();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
