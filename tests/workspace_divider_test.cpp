// Unit tests for the pure workspace/app section-divider decision
// (tests/workspace_divider.h).
//
// Pins the semantics of the "showWorkspaceDivider" toggle that replaced the old
// 0..100 "separatorOpacity": the gold divider pill is drawn ONLY when the toggle
// is ON and BOTH pin groups are populated. Self-contained (own main()); compiled
// independently by run_tests.ps1.

#include "workspace_divider.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_off_never_draws() {
    // Toggle OFF: never drawn, regardless of how many pins exist.
    CHECK(!ShouldDrawWorkspaceDivider(false, 0, 0));
    CHECK(!ShouldDrawWorkspaceDivider(false, 3, 4));
    CHECK(!ShouldDrawWorkspaceDivider(false, 5, 1));
}

static void test_on_needs_both_groups() {
    // Toggle ON but only ONE group populated: nothing to divide -> not drawn.
    CHECK(!ShouldDrawWorkspaceDivider(true, 0, 0));   // empty dock
    CHECK(!ShouldDrawWorkspaceDivider(true, 3, 0));   // only workspace pins
    CHECK(!ShouldDrawWorkspaceDivider(true, 0, 4));   // only app pins
}

static void test_on_with_both_groups_draws() {
    // Toggle ON and BOTH groups populated: the divider is drawn.
    CHECK(ShouldDrawWorkspaceDivider(true, 1, 1));
    CHECK(ShouldDrawWorkspaceDivider(true, 3, 4));
}

static void test_is_a_boolean_toggle() {
    // The whole point of the fix: it is now a plain on/off toggle. Any "on"
    // draws exactly like any other "on"; there is no 1-vs-100 gradation left.
    bool onA = ShouldDrawWorkspaceDivider(true, 2, 2);
    bool onB = ShouldDrawWorkspaceDivider(true, 9, 9);
    CHECK(onA == onB);
    CHECK(onA == true);
}

int main() {
    test_off_never_draws();
    test_on_needs_both_groups();
    test_on_with_both_groups_draws();
    test_is_a_boolean_toggle();

    if (g_failures == 0) {
        std::printf("workspace_divider_test: all checks passed\n");
        return 0;
    }
    std::printf("workspace_divider_test: %d check(s) FAILED\n", g_failures);
    return 1;
}
