// Pure gate: should THIS explorer.exe process create the dock overlay and run
// the drag resolver?
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp) and its unit tests (single_instance_gate_test.cpp),
// so the decision is verified with no Win32 deps.
//
// Bug this pins: the mod declares `// @include explorer.exe`, so Wh_ModInit runs
// in EVERY explorer.exe process. It then unconditionally called
// CreateOverlayWindow() and started the worker/resolver. Windows routinely runs
// more than one explorer.exe -- a File Explorer window opened by the mod's own
// RestoreExplorerWindowGroup (ShellExecuteEx "explorer.exe /n,...") or by the
// "open folder windows in a separate process" shell option spawns a second
// explorer.exe. That second process injected the mod and drew a SECOND dock:
//
//     PID 5936  RepositionOverlay w=260   |  PID 16032 RepositionOverlay w=227
//     PID 16032 [DRAG] RESOLVER: dock icon |  PID 5936  RESOLVER MISS: all layers failed
//
// Two overlays with different geometry (the duplicate dock) and two resolvers
// that disagree, so every subsequent drag is rejected ("breaks all future
// actions").
//
// Fix contract: only the ONE explorer.exe process that owns the real taskbar
// (Shell_TrayWnd) may create the dock. Every other explorer.exe must init as a
// no-op. The shell/taskbar owner is identified by the PID that owns
// Shell_TrayWnd (GetWindowThreadProcessId on FindWindow("Shell_TrayWnd")).
// The pure decision below is just that PID comparison; the Win32 lookup lives
// in the mod (ProcessOwnsTaskbar()).

#ifndef TASKBAR_QUICK_PIN_SINGLE_INSTANCE_GATE_H
#define TASKBAR_QUICK_PIN_SINGLE_INSTANCE_GATE_H

// True only when this process should own the dock: the taskbar owner PID was
// resolved (non-zero) AND it is this process. A zero owner PID means the taskbar
// could not be resolved yet -- never claim ownership on an unknown owner, or two
// processes that both fail to resolve would both draw a dock.
//
// currentPid       : GetCurrentProcessId() of this explorer.exe.
// taskbarOwnerPid  : PID owning Shell_TrayWnd, or 0 if it could not be resolved.
static inline bool ProcessShouldOwnDock(unsigned long currentPid,
                                        unsigned long taskbarOwnerPid) {
    return taskbarOwnerPid != 0UL && taskbarOwnerPid == currentPid;
}

#endif  // TASKBAR_QUICK_PIN_SINGLE_INSTANCE_GATE_H
