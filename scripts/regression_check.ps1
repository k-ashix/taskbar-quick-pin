<#
    regression_check.ps1  --  One-command regression gate for taskbar-quick-pin.

    Location: scripts\. Run from anywhere:

        powershell -ExecutionPolicy Bypass -File scripts\regression_check.ps1

    --------------------------------------------------------------------------
    What this does (and why it exists on top of run_tests.ps1)
    --------------------------------------------------------------------------
    taskbar-quick-pin.wh.cpp is a Windhawk mod: it #includes Win32/Windhawk
    headers and has no standalone main(), so the FULL translation unit cannot be
    compiled or executed here. run_tests.ps1 covers every pure decision that was
    MIRRORED into tests\*.h. But some fixes are STRUCTURAL properties of the mod
    source itself (an include must be first; a deleted logging layer must stay
    deleted; a busy-loop guard must stay present). A unit test cannot see those,
    so a regression there would slip through.

    This script is the regression gate for BOTH kinds of check:

      PART 1  Run the entire unit-test suite (delegates to run_tests.ps1).
              Any build/test failure fails the gate.

      PART 2  Assert source-level invariants over taskbar-quick-pin.wh.cpp --
              the things previous rounds fixed that must never silently come
              back. Each is a named MUST / MUST-NOT check with a clear message.

    Exit code is 0 only when EVERYTHING passes, so it drops straight into CI
    (see .github/workflows if present) as a required status check.

    Options:
        -SkipUnitTests   run only the source-invariant checks (PART 2)
        -Compiler g++    forwarded to run_tests.ps1 (default: g++)
        -Std c++17       forwarded to run_tests.ps1 (default: c++17)
#>

[CmdletBinding()]
param(
    [switch]$SkipUnitTests,
    [string]$Compiler = "g++",
    [string]$Std = "c++17"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$ModFile   = Join-Path $RepoRoot "taskbar-quick-pin.wh.cpp"
$ReadmeFile = Join-Path $RepoRoot "README.md"

$fail = 0
function Ok   ($m) { Write-Host ("  [PASS] " + $m) -ForegroundColor Green }
function Bad  ($m) { Write-Host ("  [FAIL] " + $m) -ForegroundColor Red; $script:fail++ }
function Head ($m) { Write-Host ""; Write-Host $m -ForegroundColor Cyan }

# Count regex matches (single-line) across the whole mod source.
$modText = Get-Content -Path $ModFile -Raw
function CountMatches ($pattern) {
    return ([regex]::Matches($modText, $pattern)).Count
}
# A "MUST NOT appear at all" check.
function AssertAbsent ($label, $pattern) {
    $n = CountMatches $pattern
    if ($n -eq 0) { Ok ("{0} -- absent (0)" -f $label) }
    else          { Bad ("{0} -- expected 0, found {1}" -f $label, $n) }
}
# A "MUST appear at least once" check.
function AssertPresent ($label, $pattern) {
    $n = CountMatches $pattern
    if ($n -ge 1) { Ok ("{0} -- present ({1})" -f $label, $n) }
    else          { Bad ("{0} -- expected >=1, found 0" -f $label) }
}

Write-Host "=== taskbar-quick-pin regression gate ===" -ForegroundColor White

# --------------------------------------------------------------------------
# PART 1  --  full unit-test suite
# --------------------------------------------------------------------------
if (-not $SkipUnitTests) {
    Head "PART 1: unit-test suite (run_tests.ps1)"
    $runner = Join-Path $ScriptDir "run_tests.ps1"
    if (-not (Test-Path $runner)) {
        Bad "run_tests.ps1 not found -- cannot run unit tests"
    } else {
        # Run ONLY the unit tests here. run_tests.ps1 now auto-chains the
        # balance check, the log-layer guard, AND this regression gate; if we
        # let it do that it would re-invoke THIS script -> infinite recursion.
        # So suppress all of its post-test sub-checks. This gate runs its own
        # source invariants (PART 2) directly below. Forward compiler/std.
        & powershell -ExecutionPolicy Bypass -File $runner `
            -SkipBalanceCheck -SkipLogLayerCheck -SkipRegressionCheck `
            -Compiler $Compiler -Std $Std
        if ($LASTEXITCODE -eq 0) { Ok "unit-test suite passed" }
        else                     { Bad ("unit-test suite failed (exit {0})" -f $LASTEXITCODE) }
    }
} else {
    Head "PART 1: unit-test suite  (skipped via -SkipUnitTests)"
}

# --------------------------------------------------------------------------
# PART 2  --  source-level invariants (things prior fixes established)
# --------------------------------------------------------------------------

Head "PART 2a: header ordering (initguid must be first include)"
# <initguid.h> defines DEFINE_GUID to EMIT storage; it must precede every other
# header or the CLSID/IID GUIDs get declared (extern) before it, breaking linkage.
$firstInclude = [regex]::Match($modText, '(?m)^\s*#include\s*[<"][^>"]+[>"]')
if ($firstInclude.Success -and $firstInclude.Value -match 'initguid\.h') {
    Ok "first #include is <initguid.h>"
} else {
    Bad ("first #include is NOT <initguid.h> (was: '{0}')" -f ($firstInclude.Value).Trim())
}

Head "PART 2b: redundant logging layer stays deleted"
# The second in-mod logging switch (on top of Windhawk's own Wh_Log gate) was
# removed. None of these may reappear.
AssertAbsent "g_debugLogging global/flag" 'g_debugLogging'
AssertAbsent "DEBUG_LOG macro/call"       '\bDEBUG_LOG\b'
AssertAbsent "TRACE_LOG macro/call"       '\bTRACE_LOG\b'
AssertAbsent "LOG_ERROR macro/call"       '\bLOG_ERROR\b'
AssertAbsent "LOG_IMPORTANT macro/call"   '\bLOG_IMPORTANT\b'
AssertAbsent "LOG_RATE macro/call"        '\bLOG_RATE\b'
AssertAbsent "DragDebugLog helper/call"        '\bDragDebugLog\b'
AssertAbsent "DragTraceVerboseLog helper/call" '\bDragTraceVerboseLog\b'
AssertAbsent "debugLogging setting key"        'debugLogging'
# The one intentional state-transition helper stays.
AssertPresent "DragTraceLog (kept) helper/calls" '\bDragTraceLog\b'
# Logging must go straight to Wh_Log now.
AssertPresent "Wh_Log direct calls" '\bWh_Log\s*\('

Head "PART 2c: Issue 2 -- unsupported-layout branch is not a busy loop"
# The branch must park on the exit event and release the hi-res timer, not
# fall straight back to the 0ms poll at the loop head.
AssertPresent "unsupported branch waits on g_exitEvent" 'g_layoutUnsupported[\s\S]{0,120}WaitForSingleObject\(g_exitEvent'
AssertPresent "unsupported branch releases hi-res timer" 'g_layoutUnsupported[\s\S]{0,120}SetHighResTimer\(false\)'

Head "PART 2d: Issue 3 -- visibility gated on VALID GEOMETRY (no ownership gate)"
# As a tool mod there is no Explorer-ownership gate any more; visibility is gated
# purely on valid geometry. RepositionOverlay must keep both windows hidden until
# the worker has produced a real dock width (g_dockLocalW > 0) -- so no 1/255-alpha
# input window sits in the top-left swallowing clicks and no mini-dock flashes on
# a cold start.
AssertPresent "RepositionOverlay stays hidden until valid geometry" 'void RepositionOverlay\(\)[\s\S]{0,2600}if \(g_dockLocalW <= 0\) \{'
# Windows 11 build gate present in the init path.
AssertPresent "Windows 11 (build >= 22000) init gate" '22000'
AssertPresent "QpIsWindows11OrGreater helper"          'QpIsWindows11OrGreater'

Head "PART 2f: rename dialog leak -- error path (got==-1) must destroy the dialog"
# PromptWorkspaceName's nested modal loop: BOTH terminating GetMessageW results
# must DestroyWindow(dlg) before leaving, or the dialog leaks a window whose
# RenameDialogState* points at a stack frame about to unwind. The WM_QUIT path
# (got==0) already destroys; this asserts the error path (got==-1) does too --
# i.e. the -1 branch is no longer a bare `break;`.
AssertPresent "rename loop destroys dialog on got==-1 (error)" 'got\s*==\s*-1\s*\)\s*\{[\s\S]{0,600}DestroyWindow\(dlg\)[\s\S]{0,40}break'

Head "PART 2g: Layer1 explorer guard uses IsExplorerExePath (not whole-path StrStrIW)"
# Resolver_Layer1_UIHit must decide "is this explorer.exe" by FILENAME
# (IsExplorerExePath -> PathFindFileNameW + _wcsicmp), not by a whole-path
# substring match (StrStrIW(result, "explorer.exe")) which false-matches paths
# that merely contain the text. Assert the correct call is present in the
# resolver and the sloppy substring form is gone.
AssertPresent "Layer1 uses IsExplorerExePath(result)" 'Resolver_Layer1_UIHit[\s\S]{0,900}IsExplorerExePath\(result\)'
AssertAbsent  "Layer1 no whole-path StrStrIW explorer.exe" 'StrStrIW\(result\.c_str\(\),\s*L"explorer\.exe"\)'

Head "PART 2e: README documents the (now Windhawk-native) logging toggle"
if (Test-Path $ReadmeFile) {
    $readme = Get-Content -Path $ReadmeFile -Raw
    if ($readme -match 'Verbose debug logging') { Ok "README has the 'Verbose debug logging' row" }
    else { Bad "README is missing the 'Verbose debug logging' settings row" }
} else {
    Bad "README.md not found"
}

Head "PART 2h: Issue 1B -- Start-button probe reports not-found (no 1/5 estimate)"
# GetStartButtonLeftEdge must return an explicit found/not-found result via an
# out-param, so an unresolved Start stays QP_LAYOUT_PENDING instead of adopting a
# fabricated tbRect.left + (tbRect.right - tbRect.left) / 5 edge. Assert the new
# bool/out-param signature exists and the old 1/5 fallback is gone.
# (Pure decision mirrored + unit-tested in tests/start_edge_probe.h.)
AssertPresent "GetStartButtonLeftEdge bool out-param signature" 'bool\s+GetStartButtonLeftEdge\s*\(\s*HWND\s+taskbar\s*,\s*const\s+RECT&\s+tbRect\s*,\s*LONG\*\s*outLeft\s*\)'
AssertAbsent  "no fabricated 1/5-width Start fallback" '\(tbRect\.right - tbRect\.left\) / 5'

Head "PART 2i: spec 2 -- tool-mode taskbar lifecycle (Explorer restart survival)"
# The tool process outlives Explorer, so HasTaskbarGeometryChanged must (a) treat
# a vanished taskbar as a change while a stale dock still exists, and (b) treat a
# brand-new Shell_TrayWnd handle as a change; and RefreshTaskbarCache must reset
# to STATE_BOOT when the taskbar is gone so the dock re-resolves cleanly on return.
# (Pure decision mirrored + unit-tested in tests/taskbar_lifecycle.h.)
AssertPresent "HasTaskbarGeometryChanged detects taskbar loss" 'if \(!tb\) return \(g_cachedTaskbar != NULL\) \|\| \(g_dockLocalW > 0\);'
AssertPresent "HasTaskbarGeometryChanged detects new Shell_TrayWnd handle" 'if \(tb != g_cachedTaskbar\) return true;'
AssertPresent "RefreshTaskbarCache resets to STATE_BOOT on taskbar loss" 'if \(!tb\) \{[\s\S]{0,700}g_systemState\s*=\s*STATE_BOOT;'

Head "PART 2j: spec 4 -- Explorer ownership machinery fully removed"
# The tool launcher's mutex (windhawk-tool-mod_<id>) is the single-instance
# mechanism now, so the old "which explorer.exe owns Shell_TrayWnd" tri-state must
# be gone entirely -- it was the reason the previous tool prototype never
# initialised.
AssertAbsent "g_dockOwnershipDecided flag"     'g_dockOwnershipDecided'
AssertAbsent "QpStartupOwnership enum"          'QpStartupOwnership'
AssertAbsent "ProbeStartupOwnership() helper"   'ProbeStartupOwnership'
AssertAbsent "QP_STARTUP_ constants"            'QP_STARTUP_'

Head "PART 2k: spec 3/6 -- tool-mod migration (header, callbacks, launcher)"
# The mod must be a Windhawk TOOL MOD: injected into windhawk.exe (not
# explorer.exe), no @architecture directive, the three callbacks renamed to
# WhTool_*, and the official launcher boilerplate pasted in.
AssertPresent "@include windhawk.exe directive" '(?m)^//\s*@include\s+windhawk\.exe'
AssertAbsent  "no @include explorer.exe directive" '(?m)^//\s*@include\s+explorer\.exe'
AssertAbsent  "no @architecture directive" '(?m)^//\s*@architecture\b'
AssertPresent "WhTool_ModInit callback"           'WhTool_ModInit'
AssertPresent "WhTool_ModSettingsChanged callback" 'WhTool_ModSettingsChanged'
AssertPresent "WhTool_ModUninit callback"         'WhTool_ModUninit'
# Official launcher boilerplate markers.
AssertPresent "tool-mod launcher flag"       'g_isToolModProcessLauncher'
AssertPresent "tool-mod dedicated-process arg" '-tool-mod'
AssertPresent "tool-mod single-instance mutex" 'windhawk-tool-mod_'
# (Launch-role decision mirrored + unit-tested in tests/tool_mod_launch_gate.h.)

Head "PART 2l: cinematic lock/unlock flash -- per-pixel-alpha glow layer"
# The old thin gold edge-stroke (DrawLockGlow, drawn on the colour-key overlay)
# is replaced by a dramatic flash on its OWN per-pixel-alpha layered window, with
# distinct LOCK (gold seal-in) vs UNLOCK (green release) effects. The kind is
# derived from g_iconsLocked at trigger time, so NO 3x P/U/L gesture code was
# touched. (Colour ramp / alpha envelope / geometry mirrored + unit-tested in
# tests/lock_glow.h + tests/lock_glow_test.cpp.)
AssertAbsent  "old thin-line DrawLockGlow retired"       'DrawLockGlow'
AssertPresent "glow: distinct SEAL (lock) kind"          'LOCKGLOW_SEAL'
AssertPresent "glow: distinct RELEASE (unlock) kind"     'LOCKGLOW_RELEASE'
AssertPresent "glow: kind derived from lock state"       'g_lockGlowKind\s*=\s*LockGlowKindFromLocked\('
AssertPresent "glow: per-pixel-alpha surface helper"     'EnsureLockGlowSurface'
AssertPresent "glow: per-pixel-alpha frame renderer"     'RenderLockGlow'
AssertPresent "glow: presented via UpdateLayeredWindow"  'UpdateLayeredWindow\(g_lockGlowWnd'
AssertPresent "glow: own click-through window class"     'QPDockLockGlow'
AssertPresent "glow: window torn down in uninit"         'DestroyWindow\(g_lockGlowWnd\)'
AssertPresent "glow: DIB freed in uninit"                'DeleteObject\(g_lockGlowDIB\)'

Head "PART 2m: glow confined to the dock (inside-only, rounded, calmer) + no dead code"
# The glow now stays INSIDE the dock rect (no outward margin), follows the dock's
# real rounded corners, softens the peak, and offers a left->right sweep vs an
# edge-only highlight chosen by the enableLockAnimation setting (default OFF).
# The old outward-spill ring (LOCK_GLOW_MARGIN) and unused LockGlowExpand01 are gone.
AssertAbsent  "glow: outward-spill margin removed"       'LOCK_GLOW_MARGIN'
AssertAbsent  "glow: dead LockGlowExpand01 removed"      'LockGlowExpand01'
AssertPresent "glow: two render modes (sweep/edge)"      'LockGlowModeFromSetting'
AssertPresent "glow: left->right sweep progress"         'LockGlowSweepX01'
AssertPresent "glow: confinement predicate"              'LockGlowPixelAllowed'
AssertPresent "glow: rounded-corner clip"                'LockGlowRoundRectSD'
AssertPresent "glow: softened peak alpha"                'LOCKGLOW_PEAK_ALPHA'
AssertPresent "glow: animation setting (default OFF)"    'enableLockAnimation'

Head "PART 2n: fullscreen-app suppression hides the whole dock"
# Like the taskbar, the dock (overlay + input + glow) fully hides while a
# fullscreen app / exclusive presentation / secure snip overlay owns the screen,
# with anti-flicker hysteresis so reappearing is seamless. Pure decision mirrored
# in tests/fullscreen_suppress.h.
AssertPresent "fullscreen: pure suppression decision"    'ShouldSuppressForFullscreen'
AssertPresent "fullscreen: anti-flicker restore gate"    'FullscreenHideDecision'
AssertPresent "fullscreen: sampled each poll"            'UpdateFullscreenState'
AssertPresent "fullscreen: shell state query"            'SHQueryUserNotificationState'
AssertPresent "fullscreen: gate acts on latched flag"    'g_fullscreenActive'

# --------------------------------------------------------------------------
# Verdict
# --------------------------------------------------------------------------
Write-Host ""
if ($fail -eq 0) {
    Write-Host "REGRESSION GATE: PASS (all checks green)" -ForegroundColor Green
    exit 0
} else {
    Write-Host ("REGRESSION GATE: FAIL ({0} check(s) failed)" -f $fail) -ForegroundColor Red
    exit 1
}
