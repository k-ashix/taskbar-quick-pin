// Pure, Win32-free decision logic for hiding the whole dock while a fullscreen
// app is in the foreground (mirrored 1:1 in taskbar-quick-pin.wh.cpp).
//
// The dock overlay + input window are HWND_TOPMOST, so without this gate they
// float on top of fullscreen apps (a game, a maximised YouTube/F11 browser,
// exclusive video) AND on top of transient secure overlays such as the
// Snipping Tool screen-clip. Windows hides the taskbar itself in exactly these
// situations; we mirror that: detect "the foreground window owns its entire
// monitor and is not the shell/desktop", OR the shell reports an exclusive
// D3D / presentation state, and suppress the dock.
//
// The actual window enumeration + SHQueryUserNotificationState call live in the
// mod; here we pin the pure boolean decision and the anti-flicker hysteresis
// that keeps enter/exit from feeling like a "hide then reappear" blink.

#ifndef TASKBAR_QUICK_PIN_FULLSCREEN_SUPPRESS_H
#define TASKBAR_QUICK_PIN_FULLSCREEN_SUPPRESS_H

// Raw signals sampled from Win32 (kept as plain bools so this is testable):
//   foregroundCoversMonitor : the foreground window's rect covers its whole
//                             monitor (within a small tolerance).
//   foregroundIsShell       : the foreground window is the desktop / shell /
//                             our own dock (NEVER treat these as fullscreen).
//   shellReportsFullscreen  : SHQueryUserNotificationState() reported an
//                             exclusive-fullscreen / presentation state.
// Returns true when the dock should be suppressed (fully hidden) this frame.
static inline bool ShouldSuppressForFullscreen(bool foregroundCoversMonitor,
                                               bool foregroundIsShell,
                                               bool shellReportsFullscreen) {
    if (shellReportsFullscreen) return true;              // exclusive D3D / presentation
    return foregroundCoversMonitor && !foregroundIsShell; // borderless / F11 fullscreen app
}

// Anti-flicker hysteresis so a single stray frame can't blink the dock:
//   * suppress IMMEDIATELY when fullscreen is detected (never let one dock
//     frame paint over a game / snip overlay),
//   * but only RESTORE the dock after `restoreDelayFrames` consecutive
//     not-suppressed samples, so a one-frame gap during an app's own mode
//     switch doesn't pop the dock in and straight back out.
// `prevActive` is whether the dock was ALREADY hidden on the previous sample;
// `missCount` is the running count of consecutive not-suppressed samples (the
// caller keeps it between frames). Returns true if the dock should be hidden
// now. The restore delay only EXTENDS an existing hide: when the dock was not
// already hidden and nothing is fullscreen, it never manufactures a hide -- so
// a fresh boot (prevActive=false, missCount=0) does not blink hidden-then-shown,
// and opening Start/Search can't briefly hide+reveal the dock.
static inline bool FullscreenHideDecision(bool suppressNow, bool prevActive,
                                          int missCount, int restoreDelayFrames) {
    if (suppressNow) return true;                 // detected -> hide at once
    if (!prevActive) return false;                // not already hidden -> never invent a hide
    return missCount < restoreDelayFrames;        // keep an EXISTING hide until it's been clear a while
}

// Edge classification of the latched suppression flag across two consecutive
// samples. Lets the mod emit EXACTLY ONE log line when the dock starts hiding
// for a fullscreen app / snip overlay (FS_ENTERED) and one when it clears and
// is about to be restored (FS_CLEARED) -- never a per-frame stream while it
// stays hidden (FS_NONE). This is the diagnostic trail for a "stuck hidden"
// dock; it does not itself change any visibility behaviour.
enum FullscreenTransition { FS_NONE, FS_ENTERED, FS_CLEARED };
static inline FullscreenTransition
FullscreenTransitionEvent(bool prevActive, bool nowActive) {
    if (nowActive && !prevActive) return FS_ENTERED;   // just started hiding
    if (!nowActive && prevActive) return FS_CLEARED;    // just cleared -> restore
    return FS_NONE;                                     // steady state, no edge
}

#endif  // TASKBAR_QUICK_PIN_FULLSCREEN_SUPPRESS_H
