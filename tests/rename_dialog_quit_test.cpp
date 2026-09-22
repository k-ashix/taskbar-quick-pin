// Unit tests for DecideRenameLoopAction: the rename-dialog nested-loop shutdown
// decision (roadmap Issue 6).
//
// The bug: the nested GetMessageW loop used `while (GetMessageW(...) > 0)`, which
// silently swallowed WM_QUIT (GetMessage returns 0 and consumes it). The outer
// UiThreadProc pump then never saw WM_QUIT and Wh_ModUninit's join hung. The fix
// re-posts WM_QUIT on the 0 result so shutdown always completes.

#include "rename_dialog_quit.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// ---- The bug: WM_QUIT (GetMessage == 0) must trigger a re-post, not a plain
//      exit that drops the quit ----------------------------------------------

static void test_wm_quit_repost() {
    CHECK(DecideRenameLoopAction(/*getMessageResult=*/0) == RENAME_LOOP_QUIT_REPOST);
}

// ---- A normal message keeps the modal loop pumping -------------------------

static void test_normal_message_pumps() {
    CHECK(DecideRenameLoopAction(/*getMessageResult=*/1) == RENAME_LOOP_PUMP);
    CHECK(DecideRenameLoopAction(/*getMessageResult=*/42) == RENAME_LOOP_PUMP);
}

// ---- A GetMessage error (-1) stops the loop WITHOUT re-posting WM_QUIT ------

static void test_error_stops_without_repost() {
    CHECK(DecideRenameLoopAction(/*getMessageResult=*/-1) == RENAME_LOOP_STOP);
}

int main() {
    test_wm_quit_repost();
    test_normal_message_pumps();
    test_error_stops_without_repost();

    if (g_failures == 0) {
        std::printf("rename_dialog_quit: ALL PASS\n");
        return 0;
    }
    std::printf("rename_dialog_quit: %d FAILURE(S)\n", g_failures);
    return 1;
}
