// Unit tests for the pure dock-border colour decision (dock_border.h).
//
// Pins the mapping from the "hideDockBorder" setting to the DWMWA_BORDER_COLOR
// value handed to DWM: ON => colourless (no grey Windows 11 outline around the
// dock), OFF => system default. Self-contained (own main()); compiled
// independently by run_tests.ps1.

#include "dock_border.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_hide_border_is_colorless() {
    // Setting ON: DWM must be told to draw NO border, so the grey outline
    // Windows 11 paints around the rounded dock disappears.
    CHECK(DockBorderColor(true) == DWMWA_COLOR_NONE);
    CHECK(DockBorderColor(true) == 0xFFFFFFFEu);
}

static void test_show_border_is_system_default() {
    // Setting OFF (default): keep the system default border -- nothing changes
    // for users who don't opt in.
    CHECK(DockBorderColor(false) == DWMWA_COLOR_DEFAULT);
    CHECK(DockBorderColor(false) == 0xFFFFFFFFu);
}

int main() {
    test_hide_border_is_colorless();
    test_show_border_is_system_default();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
