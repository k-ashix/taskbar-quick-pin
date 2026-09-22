// Pure model of the rename-dialog nested-loop shutdown decision (roadmap Issue 6).
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp, PromptWorkspaceName) and its unit tests
// (rename_dialog_quit_test.cpp), so the "what must the nested modal loop do
// with each GetMessage result" decision is verified with no Win32 deps.
//
// Background (the bug this fixes):
//   PromptWorkspaceName runs a NESTED GetMessageW loop while the rename dialog
//   is open. GetMessageW returns 0 when it dequeues WM_QUIT -- and it CONSUMES
//   that WM_QUIT. The old loop was `while (GetMessageW(...) > 0)`, so on WM_QUIT
//   it simply exited and the WM_QUIT was gone. The OUTER UiThreadProc pump then
//   never saw WM_QUIT, its GetMessageW blocked forever, and Wh_ModUninit's
//   INFINITE thread-join hung -- disabling/reloading the mod while the rename
//   dialog was open froze Explorer's UI thread.
//
// Fix contract: when the nested loop sees WM_QUIT (GetMessage == 0) it must
// destroy the dialog, RE-POST WM_QUIT (so the outer pump receives it), and stop
// the nested loop. GetMessage == -1 (error) also stops the loop. Otherwise it
// keeps pumping.

#ifndef TASKBAR_QUICK_PIN_RENAME_DIALOG_QUIT_H
#define TASKBAR_QUICK_PIN_RENAME_DIALOG_QUIT_H

// What the nested modal loop should do for a given GetMessageW return value.
enum RenameLoopAction {
    RENAME_LOOP_PUMP = 0,       // got > 0 : dispatch the message, keep looping
    RENAME_LOOP_QUIT_REPOST,    // got == 0 (WM_QUIT): destroy dialog + re-post WM_QUIT + stop
    RENAME_LOOP_STOP            // got == -1 (error): stop the loop, no re-post
};

// getMessageResult mirrors GetMessageW's BOOL return: >0 message, 0 WM_QUIT,
// -1 error.
static inline RenameLoopAction DecideRenameLoopAction(int getMessageResult) {
    if (getMessageResult == 0)  return RENAME_LOOP_QUIT_REPOST;  // WM_QUIT -- must re-post
    if (getMessageResult < 0)   return RENAME_LOOP_STOP;         // error
    return RENAME_LOOP_PUMP;                                     // normal message
}

// Whether the nested modal loop, when it STOPS looping for a given action, must
// DestroyWindow(dlg) first. Contract (leak fix): both terminating actions --
// WM_QUIT (RENAME_LOOP_QUIT_REPOST) and error (RENAME_LOOP_STOP) -- exit the
// nested loop while the dialog's RenameDialogState* still points at a stack
// frame that is about to unwind. Both MUST tear the dialog down before leaving.
// Only RENAME_LOOP_PUMP keeps the dialog alive (the loop continues). The old
// code destroyed the dialog on WM_QUIT but broke out of the -1 error path
// WITHOUT destroying it, leaking a window bound to a dead stack frame.
static inline bool RenameLoopShouldDestroyDialog(RenameLoopAction action) {
    return action == RENAME_LOOP_QUIT_REPOST || action == RENAME_LOOP_STOP;
}

#endif  // TASKBAR_QUICK_PIN_RENAME_DIALOG_QUIT_H
