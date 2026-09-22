// Unit tests for SourcePassesValidation (Stage 4 - Source Validation).
//
// Guards "fail closed": the expensive resolver runs ONLY for a live, non-system
// window. A null, stale, or shell-surface candidate must be rejected without
// ever touching the resolver / UIA / OpenProcess.

#include "source_validation.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void test_valid_live_non_system_window_passes() {
    CHECK(SourcePassesValidation(/*hasHandle=*/true,
                                 /*windowStillLive=*/true,
                                 /*isSystemWindow=*/false) == true);
}

static void test_null_handle_fails_closed() {
    // Non-dock press captured nothing usable.
    CHECK(SourcePassesValidation(false, true, false) == false);
    CHECK(SourcePassesValidation(false, false, false) == false);
}

static void test_stale_window_fails_closed() {
    // Window closed between press and drop; handle no longer valid.
    CHECK(SourcePassesValidation(true, /*windowStillLive=*/false, false) == false);
}

static void test_system_window_fails_closed() {
    // Taskbar / Start / shell surface can never be pinned -- never resolve it.
    CHECK(SourcePassesValidation(true, true, /*isSystemWindow=*/true) == false);
}

static void test_any_single_failure_fails_closed() {
    // If ANY guard fails, the whole check fails -- no "2 of 3" pass-through.
    CHECK(SourcePassesValidation(true, false, true) == false);
    CHECK(SourcePassesValidation(false, true, true) == false);
    CHECK(SourcePassesValidation(false, false, false) == false);
}

int main() {
    test_valid_live_non_system_window_passes();
    test_null_handle_fails_closed();
    test_stale_window_fails_closed();
    test_system_window_fails_closed();
    test_any_single_failure_fails_closed();

    if (g_failures == 0) {
        std::printf("source_validation: ALL PASS\n");
        return 0;
    }
    std::printf("source_validation: %d FAILURE(S)\n", g_failures);
    return 1;
}
