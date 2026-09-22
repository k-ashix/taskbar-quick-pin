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

// ---- The leak: BOTH terminating paths must destroy the dialog before leaving
//      the nested loop. The old code destroyed it on WM_QUIT (0) but broke out
//      of the -1 error path WITHOUT destroying it, leaking a window whose
//      RenameDialogState* pointed at a stack frame about to unwind. ----------

static void test_wm_quit_destroys_dialog() {
    CHECK(RenameLoopShouldDestroyDialog(DecideRenameLoopAction(/*got=*/0)) == true);
}

static void test_error_also_destroys_dialog() {
    // This is the regression the review flagged: -1 (error) must also destroy.
    CHECK(RenameLoopShouldDestroyDialog(DecideRenameLoopAction(/*got=*/-1)) == true);
}

static void test_pump_keeps_dialog_alive() {
    CHECK(RenameLoopShouldDestroyDialog(DecideRenameLoopAction(/*got=*/1)) == false);
}

int main() {
    test_wm_quit_repost();
    test_normal_message_pumps();
    test_error_stops_without_repost();
    test_wm_quit_destroys_dialog();
    test_error_also_destroys_dialog();
    test_pump_keeps_dialog_alive();

    if (g_failures == 0) {
        std::printf("rename_dialog_quit: ALL PASS\n");
        return 0;
    }
    std::printf("rename_dialog_quit: %d FAILURE(S)\n", g_failures);
    return 1;
}
