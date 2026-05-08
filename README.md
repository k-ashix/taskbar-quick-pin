# Left Taskbar Quick Pin Dock

[![Windhawk Mod](https://img.shields.io/badge/Windhawk_Mod-taskbar--quick--pin-blue.svg)](https://windhawk.net/mods/taskbar-quick-pin)
[![Version](https://img.shields.io/badge/version-v1.0.0-success.svg)](#)

A lightweight, persistent icon dock anchored just left of the Start button on the Windows 11 taskbar. Pin any app by dragging it onto the dock; click to launch or focus; double-right-click to unpin.

This mod is a single-file C++ injection into `explorer.exe`. No external DLLs. No separate service. No installer.

---

## Table of Contents

1. [What This Mod Does](#1-what-this-mod-does)
2. [Quick-Start Usage](#2-quick-start-usage)
3. [All User Gestures](#3-all-user-gestures)
4. [Settings Reference](#4-settings-reference)
5. [Architecture Overview](#5-architecture-overview)
6. [Thread Model](#6-thread-model)
7. [Startup Sequence and Boot Geometry](#7-startup-sequence-and-boot-geometry)
8. [The Overlay Window](#8-the-overlay-window)
9. [App Identity — The Three-Layer Resolver](#9-app-identity--the-three-layer-resolver)
10. [Hover Pre-Sampling](#10-hover-pre-sampling)
11. [Drag State Machine](#11-drag-state-machine)
12. [Reorder State Machine](#12-reorder-state-machine)
13. [Ghost Window](#13-ghost-window)
14. [Click Handling — SmartLaunch](#14-click-handling--smartlaunch)
15. [Running-State Detection](#15-running-state-detection)
16. [Unpin Gestures in Detail](#16-unpin-gestures-in-detail)
17. [Hotkey System](#17-hotkey-system)
18. [Persistence — Registry Storage](#18-persistence--registry-storage)
19. [Animation System](#19-animation-system)
20. [Paint Pipeline](#20-paint-pipeline)
21. [Separator Rendering](#21-separator-rendering)
22. [Glass/Blur Background](#22-glassblur-background)
23. [Multi-Monitor Support](#23-multi-monitor-support)
24. [Auto-Hide Taskbar Sync](#24-auto-hide-taskbar-sync)
25. [Dock Position Glide Animation](#25-dock-position-glide-animation)
26. [Geometry Stabilisation FSM](#26-geometry-stabilisation-fsm)
27. [Safety Guards and Exclusions](#27-safety-guards-and-exclusions)
28. [Critical Section Discipline](#28-critical-section-discipline)
29. [GDI Resource Lifecycle](#29-gdi-resource-lifecycle)
30. [Windhawk Hooks Used](#30-windhawk-hooks-used)
31. [Compiler Flags and Dependencies](#31-compiler-flags-and-dependencies)
32. [Changelog](#32-changelog)
33. [Contributing](#33-contributing)

---

## 1. What This Mod Does

This mod adds a persistent mini icon dock directly to the left of the Start button on the Windows 11 taskbar. Unlike the taskbar's own pinned-apps strip, this dock:

- Is **your own separate list** — it does not interact with the taskbar's pinned apps.
- Pins **any executable**, not just Store apps or shortcuts.
- Uses **drag-to-pin** instead of a right-click menu — drop any running app window onto the dock area.
- Responds to **left-click to launch or focus** — if the app is already running, it brings its window to front; otherwise it launches a new instance.
- **Reorders by drag** — drag a pinned icon left or right within the dock to rearrange.
- **Drag-off to unpin** — drag a pinned icon downward off the dock edge to remove it.
- Stores everything in the registry — survives explorer restarts, sign-outs, and reboots.

The dock is visually a layered transparent window that sits on top of the taskbar. It does not resize or move the taskbar. It is not a separate window outside the taskbar — it is always within the taskbar's visual bounds.

---

## 2. Quick-Start Usage

1. Install the mod through Windhawk (search for `taskbar-quick-pin` or load the `.cpp` file manually).
2. The dock appears left of Start immediately. It starts empty.
3. Open any app you want to pin (e.g. Notepad, Chrome, VS Code).
4. Click and hold that app's titlebar or taskbar button, drag toward the dock area. A ghost icon follows your cursor.
5. Release inside the dock. The icon appears with a fade-in and slide-in animation.
6. Left-click the icon to launch or focus that app at any time.
7. Double-right-click to unpin.

---

## 3. All User Gestures

| Gesture | Where | Result |
|---|---|---|
| Drag any app window or taskbar button → release on dock | Anywhere on screen | Pins the app |
| Left-click pinned icon | Dock | Focus if running, launch if not |
| Drag pinned icon left/right within dock | Dock | Live reorder with animation |
| Release during reorder inside dock | Dock | Commit new order, save |
| Drag pinned icon off dock edge | Dock | Unpin with animation |
| Double-right-click pinned icon | Dock | Unpin |
| 3 rapid left-clicks on any icon | Dock | Unpin all icons |
| Ctrl+Alt+P (configurable) | Anywhere | Pin or unpin the foreground app |
| Drag any app → release off dock | Anywhere | Cancel, no change |

---

## 4. Settings Reference

All settings are exposed in the Windhawk settings panel under this mod's entry. They are compile-time constants in the C++ code, read at mod initialisation and applied immediately.

### ICON_SIZE
Size of each icon in pixels. Default: `28`. The dock height is derived from this value plus internal padding.

### ICON_PADDING
Horizontal gap between icons in pixels. Default: `9`. Increasing this spreads icons further apart. Decreasing it to 0 makes icons touch.

### MAX_PINNED_APPS
Hard upper cap on pinned apps. Default: `5`. The dock also enforces a dynamic cap: `MaxIconsFit()` calculates how many icons physically fit given the space to the left of Start. The lower of the two limits is used. When the dock is full, a red "limit flash" is displayed instead of pinning.

### SEPARATOR_OPACITY
Opacity of the vertical separator line drawn between the dock and the Start button. Range: 0–100. Default: `100`. At 0 the line is invisible. At 100 it is opaque white. Values between 1 and 99 use a DIB-based alpha-blend drawn through `UpdateLayeredWindow`.

### ENABLE_GLASS_OVERLAY
When `true`, the dock draws a semi-transparent frosted glass rectangle behind the icons using `DwmExtendFrameIntoClientArea` or a manual GDI alpha-blend fallback. Default: `true`.

### ENABLE_REORDER
Enables the drag-to-reorder feature within the dock. When `false`, dragging a pinned icon does nothing except potentially unpin it if dragged off the edge. Default: `true`.

### STARTUP_DELAY_MS
Milliseconds to wait after `Wh_ModInit` before creating the overlay window. Default: `0`. This gives Explorer time to fully paint the taskbar before the mod measures geometry. Setting this too low can result in incorrect first-frame geometry.

### HOVER_SCALE_FACTOR
Scale multiplier applied to an icon when the cursor hovers over it. Default: `1.10` (10% larger). The scale springs in fast and springs out slightly slower for a polished feel.

### ENABLE_AUTOHIDE_SYNC
When `true`, the dock monitors the taskbar's auto-hide state and hides/shows itself to match. Default: `false`. See [section 24](#24-auto-hide-taskbar-sync) for details.

### MULTI_MONITOR_DOCK
When `true`, secondary monitor taskbars also get a dock instance. Default: `false`. See [section 23](#23-multi-monitor-support).

### Hotkey settings
Three separate settings: modifier bits (Ctrl, Alt, Shift, Win combinations), the key character, and an enable flag. Default: Ctrl+Alt+P.

---

## 5. Architecture Overview

The mod is a single `.cpp` file compiled by Windhawk's embedded Clang toolchain and injected as a DLL into `explorer.exe`. It hooks no Win32 functions. It uses Windhawk's accessibility event hook system to receive taskbar geometry notifications, and registers its own overlay window class in the Explorer process.

```
explorer.exe (main thread)
  ├─ OverlayProc (WM_PAINT, WM_NCHITTEST, WM_RBUTTONUP, WM_HOTKEY, ...)
  ├─ WinEventProc (taskbar position/geometry events via SetWinEventHook)
  └─ g_cs (CRITICAL_SECTION) ─────────────────────────────────────────┐
                                                                        │
explorer.exe (worker thread = g_workerThread)                          │
  ├─ Input polling loop (GetAsyncKeyState every 8–50 ms)               │
  ├─ Drag state machine (IDLE → PRESS → DRAGGING → DROPPED → IDLE)     │
  ├─ Reorder state machine (IDLE → PRESS → DRAG_REORDER → IDLE)        │
  ├─ Animation loop (per-icon opacity, position, hover scale)          │
  └─ Reads/writes g_pinnedApps under g_cs ──────────────────────────────┘
```

The `g_pinnedApps` vector is the only shared mutable state between threads. All reads and writes go through `g_cs`. The worker thread also uses several per-frame globals (`g_dragState`, `g_hoverIndex`, `g_anyAnimationActive`) that are written exclusively by the worker thread and read by the main thread only for display — these are effectively single-writer/multiple-reader and safe on x86-64 without a lock for their individual accesses.

---

## 6. Thread Model

**Main thread (Explorer's UI thread)**
- Creates and owns the overlay window (`g_overlayWnd`).
- Processes all `WM_*` messages for the overlay (paint, hit-test, right-click, hotkey).
- Receives WinEvent notifications from accessibility hooks (taskbar move, resize).
- Never performs slow I/O. Registry saves are deferred to happen outside any critical section.

**Worker thread (`g_workerThread`)**
- Created in `Wh_ModInit`, signalled to exit via `g_exitEvent` in `Wh_ModUninit`.
- Runs a polling loop sleeping 8–50 ms between iterations.
- Owns the drag state machine entirely.
- Runs all per-icon animations (opacity, position, hover scale).
- Calls `UpdateRunningState()` every `RUNNING_STATE_CHECK_MS` (default 2000 ms) to update the running-indicator dots.
- Calls `RefreshTaskbarCache()` each iteration during the boot phase to stabilise geometry.
- **Never** directly calls Win32 window functions on the overlay, except `InvalidateRect` (which is cross-thread safe) and `ShowWindow`/`SetWindowPos` on the ghost window (which the worker thread owns).

**Cross-thread safety rules enforced throughout:**
- `g_pinnedApps` — always accessed under `g_cs`.
- `g_overlayWnd`, `g_ghostWnd` — created on main thread. Worker thread only calls `ShowWindow`/`InvalidateRect`, never `DestroyWindow`.
- `SavePinnedApps()` — always called outside `g_cs`. Acquires CS briefly internally to snapshot paths, then releases before registry I/O.
- `HitTestIcon()` — acquires `g_cs` internally; safe to call from either thread.
- `SmartLaunch()`, `LaunchApp()` — snapshot `exePath` under CS before any slow operation (EnumWindows, ShellExecuteExW).

---

## 7. Startup Sequence and Boot Geometry

The mod cannot know the taskbar's exact dimensions at injection time — Explorer may still be painting, DWM composition may not have settled, and DPI values may not have been committed. The startup sequence handles this carefully:

```
Wh_ModInit()
  │
  ├─ InitializeCriticalSection(&g_cs)
  ├─ LoadPinnedApps()           ← loads registry paths, creates icons
  │   └─ GetIconRectLocal() called with g_dockLocalW = 0
  │      → all icons placed at currentX = targetX = 0 (INTENTIONAL, fixed later)
  ├─ Sleep(STARTUP_DELAY_MS)    ← wait for Explorer to settle
  ├─ CreateOverlayWindow()      ← layered, click-through, HWND_TOPMOST
  ├─ CreateGhostWindow()        ← separate layered window for drag preview
  ├─ RegisterHotKey()
  ├─ SetWinEventHook() × N      ← geometry/position events
  └─ CreateThread(WorkerThread)

WorkerThread (first iterations):
  └─ RefreshTaskbarCache()
       ├─ STATE_BOOT: measure taskbar via UIA/FindWindow
       │   └─ first valid width found
       │       ├─ g_dockLocalW = newW          ← geometry now valid
       │       ├─ ReseatIconPositions()         ← re-place all loaded icons correctly
       │       ├─ STATE → STATE_STABILIZING
       │       └─ RepositionOverlay()
       └─ STATE_STABILIZING: wait for 2 consecutive stable reads (±10 px)
           └─ geometry confirmed
               ├─ g_dockLocalW locked
               ├─ ReseatIconPositions()         ← final position correction
               ├─ STATE → STATE_STABLE
               └─ RepositionOverlay()
```

`ReseatIconPositions()` is the key safety call: it recalculates `targetX` and `currentX` for every loaded icon using the now-valid dock width. Without it, icons loaded from the registry would all pile at position 0 until the first pin or unpin event triggered a position recalculation.

---

## 8. The Overlay Window

The overlay is a Win32 window with these styles:
- `WS_POPUP` — no titlebar, no border.
- `WS_EX_LAYERED` — enables per-pixel alpha compositing via `UpdateLayeredWindow`.
- `WS_EX_TRANSPARENT` — hit-testing passes through to the taskbar everywhere the overlay returns `HTTRANSPARENT` from `WM_NCHITTEST`.
- `WS_EX_NOACTIVATE` — clicking the overlay never steals focus from the foreground app.
- `WS_EX_TOOLWINDOW` — keeps the window off the Alt+Tab switcher and off the taskbar's own button strip.
- `HWND_TOPMOST` — ensures it renders above the taskbar's own content.

The window is exactly the width of all pinned icons plus padding, and exactly the height of the taskbar. It is positioned so its right edge aligns with the left edge of the Start button.

**`WM_NCHITTEST`:** Returns `HTCLIENT` only over icon slots (via `HitTestIcon`), `HTTRANSPARENT` everywhere else. This is what makes the overlay "invisible" to input outside the icon area — the taskbar underneath receives all other mouse events normally.

**`WM_PAINT`:** Delegates to the paint pipeline (see [section 20](#20-paint-pipeline)). Always uses a double-buffer (`g_alphaBlendDC`) to prevent tearing.

**`WM_RBUTTONUP`:** Implements the double-right-click unpin gesture with a 500 ms window between clicks.

**`WM_HOTKEY`:** Handles the pin/unpin keyboard shortcut.

---

## 9. App Identity — The Three-Layer Resolver

When the user presses the mouse button to initiate a drag, the mod must identify *which app* is being dragged. This is harder than it sounds — the taskbar uses UWP containers, process grouping, and COM objects that do not expose a simple `GetClassName` or `GetWindowLong` path.

The resolver runs a three-layer pipeline:

**Layer 0 — Dock hit test (instant)**  
If the cursor is over an existing dock icon, that slot's `exePath` is used directly. This is the path for drag-to-reorder and drag-to-unpin.

**Layer 1 — UI Automation (preferred, ~2–10 ms)**  
`IUIAutomation::ElementFromPoint` is called with the cursor position. The returned element is queried for `UIA_NamePropertyId` and `UIA_NativeWindowHandlePropertyId`. If the element corresponds to a taskbar button, the process path is extracted from the window handle. This resolves UWP apps, grouped windows, and most modern taskbar buttons reliably.

```
IUIAutomationElement* elem = nullptr;
uia->ElementFromPoint(pt, &elem);
// → get NativeWindowHandle → GetWindowThreadProcessId → OpenProcess
//   → QueryFullProcessImageNameW → resolved path
```

**Layer 2 — GetRealWindowFromPoint (fallback, ~1 ms)**  
Calls `WindowFromPoint` to find the raw Win32 window under the cursor. If the result is the overlay itself, returns `NULL` (safe — Phase 0 handles that case). Otherwise extracts the process path from the window's PID.

**Layer 3 — Active foreground window (last resort)**  
If layers 1–2 both fail (e.g. cursor is over an empty taskbar area), the resolver returns an empty string and the drag is cancelled without pinning anything. The active foreground window is deliberately **never** used as a fallback — doing so would cause the wrong app to be pinned when the user drags from a non-app taskbar area.

The full pipeline runs on the worker thread synchronously at mouse-down time, with the pre-sampled hover candidate (see section 10) injected as a zero-cost shortcut when it is fresh enough.

---

## 10. Hover Pre-Sampling

A drag takes ~150–500 ms from mouse-down to mouse-up. But the app under the cursor at press time is already known while the cursor is hovering — before the button goes down. The pre-sampler takes advantage of this:

Every worker loop iteration (8–50 ms), when the cursor is near the taskbar:
1. Run a lightweight hover check: call `HitTestIcon` and the L1/L2 resolver in read-only mode.
2. Store the result as `g_hoverCandidate` with a timestamp `g_hoverCandidateAge`.
3. At actual mouse-down (IDLE→PRESS transition), if the age is less than `STABILITY_CONFIRM_MS` (default 120 ms), use the pre-sampled result directly — no resolver call needed.

This eliminates resolver latency from the user's perspective. The drag feels instant because identity is already known before the click.

If the hover candidate is stale (cursor moved fast, or was over empty taskbar space), the resolver runs fresh at press time as normal.

---

## 11. Drag State Machine

The worker thread runs a six-state machine on every loop iteration:

```
DRAG_IDLE
  │
  │  LButton down + cursor near taskbar
  ▼
DRAG_PRESS                   ← identity resolved, hold position
  │
  ├── Mouse released quickly (< CLICK_MAX_MS, < CLICK_MAX_MOVE_PX)
  │     ▼
  │   SmartLaunch(idx)   →  DRAG_IDLE
  │
  ├── Cursor moves beyond REORDER_THRESHOLD_PX while over dock icon
  │     ▼
  │   DRAG_REORDER             ← drag-to-reorder active
  │     │
  │     ├── Cursor leaves dock magnetic zone
  │     │     ▼
  │     │   DRAG_DRAGGING (unpin path)
  │     │
  │     └── Mouse released inside dock
  │           ▼
  │         CommitReorder() → DRAG_IDLE
  │
  └── Cursor moves beyond DRAG_THRESHOLD_PX (not a dock icon)
        ▼
      DRAG_DRAGGING            ← drag-to-pin/unpin active
        │
        ├── Mouse released inside dock zone
        │     ▼
        │   PinApp(path) → DRAG_DROPPED → DRAG_IDLE
        │
        └── Mouse released outside dock zone
              ▼
            (if from dock) UnpinAppByIndex → DRAG_DROPPED → DRAG_IDLE
            (if external)  cancel → DRAG_IDLE

DRAG_CANCELLED               ← forced reset by HardStateReset
  │
  └── Mouse released
        ▼
      DRAG_IDLE
```

**Threshold constants:**

| Constant | Default | Purpose |
|---|---|---|
| `DRAG_THRESHOLD_PX` | 8 | Min cursor travel before PRESS → DRAGGING |
| `REORDER_THRESHOLD_PX` | 6 | Min travel while over icon before PRESS → REORDER |
| `CLICK_MAX_MS` | 400 | Max hold time for a click (not drag) |
| `CLICK_MAX_MOVE_PX` | 5 | Max movement for a click (not drag) |

**Hard reset:** `HardStateReset()` is called from `WM_RBUTTONUP` and from the unpin-all path. It acquires `g_cs`, clears `g_draggedAppPath`, `g_lockedDragPath`, resets indices to -1, clears the ghost, and sets state to `DRAG_IDLE`. This is the safety net for any scenario where the state machine gets stuck (e.g. rapid ESC-key or focus-switch during a drag).

---

## 12. Reorder State Machine

When the user drags a dock icon left or right within the dock, the reorder sub-machine takes over:

1. **PRESS over icon:** The source index (`g_reorderSrcIdx`) is recorded.
2. **DRAG_REORDER active:** Every loop iteration, `UpdateReorderPositions()` recalculates a "phantom" target position for every non-dragged icon, spreading them around the gap left by the dragged icon. This drives the live shuffle animation.
3. **Mouse up inside dock:** `CommitReorder()` performs the actual `std::rotate` in the vector, then recalculates all canonical positions. `SavePinnedApps()` is called outside the CS.
4. **Cursor leaves dock magnetic zone:** Transitions to `DRAG_DRAGGING` (unpin-drag path), restoring all non-source icons to their canonical positions.

`UpdateReorderPositions()` iterates through the icon list, placing each non-source icon in sequence while skipping the source slot. The target position is the canonical position for rank `nonSrc` (not rank `i`). This produces the "icons shuffle around the held icon" visual.

```
// Simplified version of UpdateReorderPositions:
int nonSrc = 0;
for (int i = 0; i < n; ++i) {
    while (nonSrc < n && nonSrc == srcIdx) nonSrc++;  // skip the dragged slot
    if (i == srcIdx) continue;
    RECT r = GetIconRectLocal(nonSrc, n);
    g_pinnedApps[i].targetX = (float)r.left;
    nonSrc++;
}
```

---

## 13. Ghost Window

During any drag (both external-to-dock and dock-to-outside), a semi-transparent icon preview follows the cursor. This is the ghost window.

The ghost is a separate `WS_EX_LAYERED | WS_EX_TOOLWINDOW | HWND_TOPMOST` window, always 48×48 px (`GHOST_SIZE`). It uses `UpdateLayeredWindow` with `AC_SRC_ALPHA` blending at 190/255 opacity.

**Lifecycle:**
1. Created once in `Wh_ModInit` (`CreateGhostWindow`).
2. Hidden (`SW_HIDE`) at all times except during `DRAG_DRAGGING`.
3. `UpdateGhostWindow(cursor)` is called every frame during dragging:
   - Validates the icon handle via `GetIconInfo` (lightweight OS call — returns FALSE for stale handles without crashing).
   - Draws the icon into a pre-allocated 48×48 DIB via `DrawIconEx`.
   - Runs `PremultiplyAlpha` over all pixels so `UpdateLayeredWindow` composites correctly.
   - Calls `UpdateLayeredWindow` to push the new pixels.
4. When drag ends (any outcome), `GhostCleanup()` is called: hides the window, destroys the DIB and DC, nulls all ghost-related globals.
5. Destroyed in `Wh_ModUninit` via `DestroyWindow`.

The ghost is only shown when the cursor is within `IsCursorInDockOrTaskbarRegion()` — a 20 px expansion of the combined taskbar+dock bounding box. Outside this zone, the ghost hides automatically so it does not float over the desktop during long drag arcs.

---

## 14. Click Handling — SmartLaunch

A left-click (press and release without meaningful movement, within `CLICK_MAX_MS`) on a dock icon calls `SmartLaunch(idx)`.

**SmartLaunch logic:**

```
SmartLaunch(idx):
  1. Snapshot exePath under g_cs (safe copy, no dangling refs)
  2. FindRunningAppWindow(path):
       EnumWindows → for each visible, non-tool window:
         GetWindowThreadProcessId → OpenProcess → GetModuleFileNameExW
         compare path (case-insensitive) → return hwnd if match
  3. If found:
       - IsIconic? → ShowWindow(SW_RESTORE)
       - SetWindowPos(HWND_TOP) to raise it in z-order
       - SetForegroundWindow + SetActiveWindow
       → return (no launch)
  4. If not found:
       - Rate-limit check: max 1 launch per 800 ms per icon
       - ShellExecuteExW(path, SW_SHOWNORMAL)
```

The running-state indicator (a small dot under each icon — see [section 15](#15-running-state-detection)) gives the user a visual cue before clicking: solid dot = running (click will focus), no dot = not running (click will launch).

---

## 15. Running-State Detection

The mod draws a small indicator dot below each icon to show whether that app has a visible window. This state is updated by a background scan that runs every `RUNNING_STATE_CHECK_MS` (default 500 ms) on the worker thread.

`UpdateRunningState()` calls `EnumWindows` to enumerate all visible, non-tool-window, non-empty-title top-level windows. For each window it opens a `PROCESS_QUERY_LIMITED_INFORMATION` handle (lowest-privilege level that supports `QueryFullProcessImageNameW`) and collects the full process path. The capacity is capped at 128 windows (stack-allocated buffer — no heap allocation on the hot path).

After enumeration, the worker acquires `g_cs` and sweeps `g_pinnedApps`, setting `app.running = true` for each app whose path matches any collected path (case-insensitive). Apps with no match get `app.running = false`.

The indicator is a 4×4 dot painted 4 px below the icon's bottom edge in `WM_PAINT`. Running apps get a white dot. Non-running apps get no dot.

---

## 16. Unpin Gestures in Detail

There are four distinct ways to unpin an icon:

**1. Double-right-click**  
`WM_RBUTTONUP` tracks two statics: `s_lastRClickIdx` and `s_lastRClickTime`. A second right-click on the same icon within 350 ms calls `UnpinAppByIndex(idx)`. Different icon or timeout resets the sequence.

**2. Drag off dock**  
From `DRAG_DRAGGING` state with `g_dragFromDock = true`: on mouse-up, if cursor is outside the dock zone (inflated by 8 px), `UnpinAppByIndex(g_dragFromDockIdx)` is called. The icon slides out and fades while dragging, and disappears on drop.

**3. Rapid 3-click**  
The rapid-click tracker maintains `g_rapidClickIndex`, `g_rapidClickCount`, and `g_rapidClickStart`. Three left-clicks on any icon within `RAPID_CLICK_WINDOW_MS` (default 1000 ms) calls `UnpinAllApps()`. The counter resets on icon change or timeout.

**4. Hotkey (toggle)**  
If the hotkey is pressed while the foreground app is already pinned, `UnpinAppByIndex` is called for that app. See [section 17](#17-hotkey-system).

**`UnpinAppByIndex()` internals:**
1. Checks `g_dragState` — refuses to unpin during `DRAG_DRAGGING` or `DRAG_PRESS` (prevents race between drag completion and unpin).
2. Acquires `g_cs`.
3. Calls `HardStateReset()` inside CS to clear any in-flight drag targeting this index.
4. Calls `DestroyIcon` on the icon handle.
5. Calls `g_pinnedApps.erase(it)`.
6. Releases CS.
7. Calls `SavePinnedApps()` outside CS.
8. Calls `InvalidateRect` on the overlay.

---

## 17. Hotkey System

The hotkey is registered with `RegisterHotKey` on the overlay window handle. Default: `MOD_CONTROL | MOD_ALT`, key `'P'`.

`WM_HOTKEY` handling:
1. `GetForegroundWindow()` to get the current foreground app.
2. Reject system windows via `IsSystemWindow()` (see [section 27](#27-safety-guards-and-exclusions)).
3. `GetProcessPath(fg)` to get the executable path.
4. Reject excluded apps via `IsExcludedApp()`.
5. Acquire `g_cs`, check `IsPinned(path)` and search for the index if pinned.
6. Release `g_cs`.
7. If already pinned → `UnpinAppByIndex(idx)`.  
   If not pinned → `PinApp(path)`.

The CS is released before calling `UnpinAppByIndex`/`PinApp` because those functions acquire it themselves. The index found in step 5 is used directly — it is stable because `WM_HOTKEY` runs on the main thread, and `UnpinAppByIndex` guards against concurrent drags.

**To disable the hotkey:** Set the modifier dropdown to "Disabled (0)" in settings.

**To change the hotkey:** Adjust the modifier bits and key separately in settings. Both take effect on next mod reinit or reload.

---

## 18. Persistence — Registry Storage

Pinned apps are stored at:

```
HKEY_CURRENT_USER\Software\WindhawkMods\taskbar-quick-pin
  Value: PinnedApps (REG_MULTI_SZ)
  Format: null-separated wide strings, double-null terminated
  Content: one full executable path per entry, in dock order
```

**`SavePinnedApps()`:**
- Acquires `g_cs` briefly to snapshot `exePath` for each entry into a `std::wstring` (the multi-sz format).
- Releases `g_cs` before the registry call (`RegCreateKeyExW` + `RegSetValueExW`).
- This ensures registry I/O never holds the animation lock.
- Called after every pin, unpin, unpin-all, and reorder.

**`LoadPinnedApps()`:**
- Runs at mod init, before geometry is stable.
- For each path in the registry: validates the path exists on disk (`GetFileAttributesW`), extracts the icon via `SHGetFileInfoW` + `CopyIcon`, and appends to `g_pinnedApps`.
- Paths that no longer exist on disk are silently dropped (no registry cleanup — orphaned entries disappear on next save).
- Icon positions are all set to 0 at this point (dock width not yet known). `ReseatIconPositions()` corrects this when geometry stabilises.

---

## 19. Animation System

Every icon has three animated properties, updated every frame by the worker thread inside `g_cs`:

**1. Opacity (`app.opacity`)**  
New icons start at 0.0 and fade in at `PIN_FADE_SPEED` (default 0.06 per 16 ms frame). Removed icons are deleted immediately on unpin — there is no fade-out (the visual change is immediate and clean).

**2. X position (`app.currentX` → `app.targetX`)**  
Uses a spring-momentum system:
```
diff     = targetX - currentX
tf       = min((frameDeltaMs / 16.0) * ICON_ANIM_SPEED, 0.5)
velocity = (velocity + diff * tf) * ANIM_MOMENTUM_DECAY
currentX += velocity
```
This produces a smooth overshoot-and-settle motion. `ICON_ANIM_SPEED = 0.18`, `ANIM_MOMENTUM_DECAY = 0.72`. New icons (`app.isNew`) start at `targetX + PIN_SLIDE_OFFSET` (10 px to the right) and slide into position.

**3. Hover scale (`app.hoverScale`)**  
Springs toward `HOVER_SCALE_FACTOR` (1.15) when `i == g_hoverIndex` and the icon is fully opaque. Springs back toward 1.0 when not hovered. Scale-in uses `HOVER_SCALE_IN_SPEED = 0.35`, scale-out uses `HOVER_SCALE_OUT_SPEED = 0.20` — slightly asymmetric for a snappy-in, graceful-out feel.

**Frame timing:**
- `g_frameDeltaMs` is measured each iteration as `GetTickCount() - g_lastFrameTime`.
- Capped at 33 ms (effectively 30 fps cap) to prevent position explosion when the system is lagging.
- Values above `FRAME_DELTA_SNAP_MS` (100 ms default) trigger snap-to-target for all animations, preventing icons from "catching up" in a jarring way after a system suspension.

**Adaptive sleep:**
- 8 ms when dragging or any animation active (≈ 125 fps effective refresh).
- 16 ms immediately after animation completes (brief cool-down).
- 50 ms when fully idle for 10+ consecutive idle frames (saves CPU to ~0%).

---

## 20. Paint Pipeline

`WM_PAINT` runs on the main thread. It uses a persistent off-screen buffer (`g_alphaBlendDC` / `g_alphaBlendBmp`) to draw all icons, then blits to the real window DC via `BitBlt`. The buffer is recreated only when the window dimensions change.

**Per-icon rendering (inside g_cs):**

1. Compute draw position: `x = dockOriginX + (int)app.currentX`, `y = dockOriginY + ICON_PADDING`.
2. Apply hover scale: scale the icon rect around its centre point.
3. Draw icon with `DrawIconEx` at `(UINT)(app.opacity * 255)` alpha via `ImageList_DrawEx` or direct `DrawIconEx` with GDI alpha tricks.
4. If `app.running`: draw a 4×4 white dot 4 px below the icon bottom.
5. If this is the drag source (`g_dragFromDock && i == g_dragFromDockIdx`): draw at 40% opacity to indicate it is "lifted."
6. If the drop zone is active (`g_dropZoneActive`): draw a highlight rect around the icon area.

**Limit flash:** When the dock is full and the user tries to pin another app, `g_limitFlashActive` is set for 2000 ms. During this time, the separator line is drawn in red (`g_linePenFlash`) instead of the normal colour.

**Drop zone highlight:** When `DRAG_DRAGGING` is active and the cursor re-enters the dock zone, a semi-transparent green overlay is drawn over the dock area to indicate a valid drop target.

---

## 21. Separator Rendering

A thin vertical line is drawn between the last dock icon and the Start button. Its appearance depends on `SEPARATOR_OPACITY`:

- **0:** Nothing drawn.
- **100:** A solid 1×(dockHeight) `LineTo` using `g_linePenNormal`.
- **1–99:** A DIB-based semi-transparent line using a cached off-screen DC (`g_sepDC` / `g_sepDIB`). The DIB is allocated once at first use and cached. It is only reallocated if the dock height changes (DPI switch or taskbar resize). The alpha-blended column is painted via `AlphaBlend` directly into the paint DC.

The separator DIB is freed in `Wh_ModUninit` via:
```cpp
SelectObject(g_sepDC, (HBITMAP)NULL);
DeleteObject(g_sepDIB);
DeleteDC(g_sepDC);
```

---

## 22. Glass/Blur Background

When `ENABLE_GLASS_OVERLAY` is true, a frosted glass rectangle is rendered behind the dock icons. The implementation attempts two approaches in order:

1. **DWM accent policy** (`DwmSetWindowAttribute` with `DWMWA_SYSTEMBACKDROP_TYPE`): Available on Windows 11 22H2+. Sets the window backdrop to `DWMSBT_TRANSIENTWINDOW` which gives the blur-behind effect used by system flyouts.

2. **Manual GDI alpha rectangle (fallback):** If DWM accent fails, a 30% opacity dark rectangle is drawn via `AlphaBlend` using a solid black DIB and a `BLENDFUNCTION` with `SourceAlpha = 77` (30% of 255).

When `ENABLE_GLASS_OVERLAY` is false, the overlay is fully transparent everywhere outside icon pixels.

---

## 23. Multi-Monitor Support

When `MULTI_MONITOR_DOCK = true`, the mod creates a secondary dock instance for each non-primary monitor that has a taskbar.

Secondary docks are lightweight — they share the same icon list (`g_pinnedApps`) and the same critical section as the primary dock. They have their own overlay windows but no worker thread of their own; the primary worker thread drives their animation and state by calling `RepaintSecondaryDocks()` at appropriate points.

Secondary dock positions are calculated by `FindSecondaryTaskbar()` for each monitor, using the same UIA measurement pipeline as the primary. Secondary docks update when the primary dock's geometry updates, on a `WM_DISPLAYCHANGE` notification.

Limitations:
- Secondary docks display the same icons as the primary — there is no per-monitor pin list.
- Drag-to-pin works on the primary monitor only. Secondary docks are display-only + click-to-launch.

---

## 24. Auto-Hide Taskbar Sync

When `ENABLE_AUTOHIDE_SYNC = true`, the dock monitors the taskbar's auto-hide state via `SHAppBarMessage(ABM_GETSTATE)` (polled in the worker thread every 5000 ms). When auto-hide is detected:

- The overlay is hidden (`ShowWindow(SW_HIDE)`) while the taskbar is scrolled off-screen.
- The overlay is shown again (`ShowWindow(SW_SHOW)`) when the taskbar slides back into view.

Detection of "taskbar visible" vs "taskbar hidden" is done by checking whether the taskbar's current Y position is within the screen's working area. This is an approximation — it does not hook the taskbar's slide animation, it simply polls.

This feature defaults to `false` because on most configurations it is not needed (the dock is within the taskbar's height and scrolls with it naturally).

---

## 25. Dock Position Glide Animation

When the dock needs to reposition (icon count changes, taskbar moves), it does not jump instantly. Instead it uses a linear glide:

```
g_dockTargetX set to new position
g_dockPosAnimActive = true

Each frame (AnimateDockPositionStep):
  diff = targetX - currentX
  if |diff| < 1.0: snap, stop animation
  else: currentX += diff * DOCK_GLIDE_SPEED (0.25 per frame)
  → RepositionOverlay()
```

`RepositionOverlay()` calls `SetWindowPos` to physically move the overlay window to the new position. This is intentionally called every animation frame during glide — each call moves the window by a few pixels, producing the smooth slide effect.

---

## 26. Geometry Stabilisation FSM

The dock needs the taskbar's width and position to calculate icon layout. This is measured by `RefreshTaskbarCache()`, which runs on the worker thread and drives a three-state FSM:

```
STATE_BOOT
  │  First valid measurement (width > MIN_VALID_DOCK_WIDTH = 40)
  ▼
STATE_STABILISING
  │  Two consecutive measurements within ±10 px of each other
  │  OR boot timeout (BOOT_PHASE_MS = 3000 ms) elapsed
  ▼
STATE_STABLE
  │  Only updates on significant change (>50 px delta)
  │  (handles resolution changes and taskbar size changes)
```

At each state transition that commits a new `g_dockLocalW`, `ReseatIconPositions()` is called to recalculate all icon `targetX`/`currentX` values. Without this, icons loaded from the registry at boot time would all have `currentX = 0` (dock width was unknown at load time).

---

## 27. Safety Guards and Exclusions

**`IsSystemWindow(hwnd)`** returns true for windows belonging to:
- Explorer itself (`explorer.exe`)
- `SearchHost.exe` (Windows Search)
- `ShellExperienceHost.exe`
- `StartMenuExperienceHost.exe`
- `LogonUI.exe`
- Windows with class `Progman` (the desktop window)
- Windows with class `Shell_TrayWnd` (the taskbar itself)
- Windows with class `WorkerW` (desktop worker windows)

**`IsExcludedApp(path)`** additionally checks the path against a hard-coded exclusion list including:
- `RuntimeBroker.exe`
- `WerFault.exe`
- `CredentialUIBroker.exe`
- `ApplicationFrameHost.exe` (UWP frame — the app inside it is used instead)

**Icon validation:** Before pinning, `SHGetFileInfoW` must succeed AND `GetIconInfo` must return a valid `hbmColor` or `hbmMask`. Apps that produce no extractable icon are silently rejected.

**Duplicate guard:** `IsPinned(path)` checks `g_pinnedApps` (under `g_cs`) before `PinApp` proceeds. Dragging the same app twice does nothing the second time.

**Capacity guard:** `MaxIconsFit()` computes `(availableWidth - ICON_PADDING) / (ICON_SIZE + ICON_PADDING)` where `availableWidth = distanceFromDockLeftEdgeToStartButton`. The effective cap is `min(MAX_PINNED_APPS, MaxIconsFit())`. A full dock shows the limit-flash and logs a rejection.

---

## 28. Critical Section Discipline

One `CRITICAL_SECTION g_cs` protects the entire `g_pinnedApps` vector and all fields within each `PinnedApp` struct. Rules:

- **All reads and writes to `g_pinnedApps`** — including `size()`, `operator[]`, `push_back`, `erase`, and iterator traversal — are done inside `EnterCriticalSection` / `LeaveCriticalSection`.
- **`SavePinnedApps()`** acquires CS only briefly to snapshot path strings, then releases before registry I/O.
- **`HitTestIcon()`** acquires CS internally. Never call with CS already held from the same thread (that would deadlock — CS is non-recursive by default).
- **`SmartLaunch()`** and **`LaunchApp()`** snapshot `exePath` under CS immediately, then release. All subsequent work (EnumWindows, ShellExecuteExW) happens outside CS.
- **Animation loop** holds CS for the full per-frame icon update (opacity, position, scale). This is intentionally a brief hold — no I/O, no window calls, pure arithmetic.
- **`WM_PAINT`** holds CS for the full paint pass. Paint is fast (sub-millisecond on any modern system). The animation loop has a complementary brief wait if paint is mid-frame.

---

## 29. GDI Resource Lifecycle

All GDI objects are created once and destroyed in `Wh_ModUninit` in reverse order.

| Resource | Created in | Destroyed in | Notes |
|---|---|---|---|
| `g_overlayWnd` | `CreateOverlayWindow` | `Wh_ModUninit` | DestroyWindow |
| `g_ghostWnd` | `CreateGhostWindow` | `Wh_ModUninit` | DestroyWindow |
| `g_ghostDIB` | `CreateGhostWindow` | `Wh_ModUninit` + `GhostCleanup` | DeleteObject |
| `g_alphaBlendDC` | First paint | `Wh_ModUninit` | DeleteDC |
| `g_alphaBlendBmp` | First paint | `Wh_ModUninit` | DeleteObject |
| `g_sepDC` | First separator paint | `Wh_ModUninit` | DeleteDC |
| `g_sepDIB` | First separator paint | `Wh_ModUninit` | DeleteObject |
| `app.icon` (per icon) | `PinApp` / `LoadPinnedApps` | `UnpinAppByIndex` / `Wh_ModUninit` | DestroyIcon |
| `g_blackBrush` | First paint | `Wh_ModUninit` | DeleteObject |
| `g_linePenNormal/Flash/Drop` | `Wh_ModInit` | `Wh_ModUninit` | DeleteObject |
| `g_runDotBrush` | `Wh_ModInit` | `Wh_ModUninit` | DeleteObject |

`Wh_ModUninit` destroys resources in a safe order: worker thread signalled and joined first, then secondary docks, then GDI objects, then the critical section. Icon handles are destroyed inside a final `g_cs` acquire before `DeleteCriticalSection`.

---

## 30. Windhawk Hooks Used

This mod uses **no Win32 API hooks**. It does not detour or patch any function. It uses only:

- **`SetWinEventHook`** (standard Win32 accessibility API) to receive `EVENT_OBJECT_LOCATIONCHANGE`, `EVENT_OBJECT_CREATE`, and `EVENT_SYSTEM_FOREGROUND` events. These are used to detect taskbar moves/resizes and to refresh geometry.
- **`RegisterHotKey`** on its own overlay window.
- **`IUIAutomation`** COM interface (standard, in-process) for the Layer 1 resolver.
- **`SHAppBarMessage`** for auto-hide state detection.

Because no hooks are installed, the mod has zero risk of causing instability from a bad hook trampoline. The worst that can happen if the mod crashes is that `explorer.exe` unloads the injected DLL and the dock disappears.

---

## 31. Compiler Flags and Dependencies

Windhawk builds the mod with Clang targeting `x86_64-pc-windows-msvc`. The `@compilerOptions` header line specifies:

```
-lpsapi       // GetModuleFileNameExW
-lshell32     // SHGetFileInfoW, ShellExecuteExW, SHAppBarMessage
-lole32       // COM initialisation (CoCreateInstance for IUIAutomation)
-loleaut32    // VARIANT/BSTR helpers used by UIA
-luuid        // IID_IUIAutomation and other interface GUIDs
-lshlwapi     // PathFindFileNameW, StrCmpIW helpers
-lgdi32       // CreateDIBSection, BitBlt, AlphaBlend, etc.
-lmsimg32     // AlphaBlend (separate lib on older SDK versions)
-luiautomationcore  // UIAutomationCore.dll — IUIAutomation implementation
```

The mod uses C++17 features (structured bindings, `if constexpr`, lambdas in functions). It requires Windows 10 v1903+ at runtime for `QueryFullProcessImageNameW` and the UIA APIs used.

---

## 32. Changelog

- Initial release of Taskbar Quick Pin Dock
---

## 33. Contributing

We welcome contributions! Whether you're fixing bugs, improving the documentation, or adding new features, here are a few ways you can help:

- **Bug Reports & Feature Requests:** Please open an issue on the repository detailing the problem or feature you have in mind.
- **Pull Requests:** When submitting code, ensure that changes align with the thread-safe architecture outlined in this document. Keep the single-file structure and confirm that GDI resources are managed properly without leaks.
- **Developer Guidelines:** The codebase is designed to have zero external dependencies and runs entirely within `explorer.exe`. Avoid hooking Win32 functions unless absolutely necessary. Be extremely careful when adding cross-thread shared state.

---

*This document describes the internal implementation of the mod as of v1.0.0. All implementation details refer to the v1.0.0 source file.*
