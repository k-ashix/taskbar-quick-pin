// Unit tests for BuildTaskbarProbes: the L2 seam-recovery retry that fixes
// "RESOLVER MISS near (1066,1391)" when a click lands between taskbar buttons.

#include "taskbar_probe.h"
#include <cstdio>
#include <cmath>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- THE BUG: an exact-point miss must produce recovery probes --------------

static void test_miss_produces_probes() {
    // The field miss: (1066,1391), taskbar buttons ~44px pitch, button row
    // center at y=1405.
    auto probes = BuildTaskbarProbes(/*missX=*/1066, /*missY=*/1391,
                                     /*pitch=*/44, /*midY=*/1405);
    CHECK(!probes.empty());
    // First probe snaps to the vertical center at the same X (closest recovery).
    CHECK(probes.front().x == 1066);
    CHECK(probes.front().y == 1405);
}

static void test_probes_never_reach_neighbor_button() {
    // No probe may be >= half a pitch away horizontally, or it would land on the
    // adjacent button and resolve the WRONG app.
    const int pitch = 44;
    auto probes = BuildTaskbarProbes(1066, 1391, pitch, 1405);
    for (const auto& p : probes)
        CHECK(std::abs(p.x - 1066) < pitch / 2);
}

static void test_probes_are_nearest_first() {
    // Horizontal probes must grow in |dx| so the closest genuine button wins.
    auto probes = BuildTaskbarProbes(1000, 1391, 48, 1405);
    int prevDist = -1;
    for (const auto& p : probes) {
        int d = std::abs(p.x - 1000);
        CHECK(d >= prevDist);   // non-decreasing distance
        prevDist = d;
    }
}

static void test_original_point_never_reprobed() {
    // The exact miss point was already tried; it must not appear again.
    const int mx = 1066, my = 1391;
    auto probes = BuildTaskbarProbes(mx, my, 44, 1405);
    for (const auto& p : probes)
        CHECK(!(p.x == mx && p.y == my));
}

static void test_unknown_pitch_yields_no_retry() {
    // If we cannot measure button spacing, do NOT guess -- return no probes so
    // the resolver keeps its "over taskbar -> UIA-only, no fallback" contract.
    CHECK(BuildTaskbarProbes(1066, 1391, /*pitch=*/0, 1405).empty());
    CHECK(BuildTaskbarProbes(1066, 1391, /*pitch=*/-5, 1405).empty());
}

static void test_no_midY_keeps_original_y() {
    // When the button-row center is unknown (<=0), probes keep the miss Y and
    // only nudge horizontally.
    auto probes = BuildTaskbarProbes(1066, 1391, 44, /*midY=*/0);
    CHECK(!probes.empty());
    for (const auto& p : probes)
        CHECK(p.y == 1391);
}

int main() {
    test_miss_produces_probes();
    test_probes_never_reach_neighbor_button();
    test_probes_are_nearest_first();
    test_original_point_never_reprobed();
    test_unknown_pitch_yields_no_retry();
    test_no_midY_keeps_original_y();

    if (g_failures == 0) {
        std::printf("taskbar_probe: ALL PASS\n");
        return 0;
    }
    std::printf("taskbar_probe: %d FAILURE(S)\n", g_failures);
    return 1;
}
