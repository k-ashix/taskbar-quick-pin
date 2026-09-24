// Pure, Win32-free decision for whether the gold workspace/app section divider
// is painted. Mirrored 1:1 from taskbar-quick-pin.wh.cpp (OverlayProc paint).
//
// Background: the old right-edge separator LINE was removed. The setting that
// used to drive it -- "separatorOpacity", a 0..100 value -- no longer has a
// line to fade, so values 1..100 all behaved identically: the ONLY thing it
// still gated was whether the gold divider pill between the workspace-pin group
// and the app-pin group is drawn. That is a boolean, so the setting became the
// boolean "showWorkspaceDivider".
//
// Behaviour being pinned: the divider is drawn ONLY when the toggle is ON and
// BOTH groups are populated -- an empty workspace region leaves no section gap,
// so there is nothing to divide.

#ifndef TASKBAR_QUICK_PIN_WORKSPACE_DIVIDER_H
#define TASKBAR_QUICK_PIN_WORKSPACE_DIVIDER_H

static inline bool ShouldDrawWorkspaceDivider(bool showWorkspaceDivider,
                                              int workspaceSlots, int appCount) {
    return showWorkspaceDivider && workspaceSlots > 0 && appCount > 0;
}

#endif  // TASKBAR_QUICK_PIN_WORKSPACE_DIVIDER_H
