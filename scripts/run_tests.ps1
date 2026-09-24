<#
    run_tests.ps1  --  Build & run all unit tests for taskbar-quick-pin.

    HOW TO RUN
    ----------
    No arguments are required. The default command runs ALL unit tests and then
    automatically runs every enabled source/guard check.

    From the repository root:

        powershell -ExecutionPolicy Bypass -File .\scripts\run_tests.ps1

    From any directory, using the full path:

        powershell -ExecutionPolicy Bypass -File "G:\8_VS\1_Completed_Zip\Taskbar_quick_pin\scripts\run_tests.ps1"

    From an existing PowerShell prompt, this shorter form also works when the
    current-user execution policy allows local scripts:

        & "G:\8_VS\1_Completed_Zip\Taskbar_quick_pin\scripts\run_tests.ps1"

    Success is reported as "OVERALL : PASS" and process exit code 0. Any unit
    test or auto-invoked guard failure reports "OVERALL : FAIL" and returns a
    non-zero exit code, so this same command is suitable for CI.

    The script locates the repository root and tests\ directory relative to its
    own file path. Therefore it does not depend on the current working directory.

    --------------------------------------------------------------------------
    Why the tests are NOT linked against taskbar-quick-pin.wh.cpp
    --------------------------------------------------------------------------
    taskbar-quick-pin.wh.cpp is a Windhawk mod: it #includes Windhawk/Win32
    headers and has no standalone main(), so it cannot be compiled on its own.
    Its comments say as much. To keep the logic testable, each pure decision is
    MIRRORED into a small header under tests\ (e.g. click_vs_drag.h), and each
    tests\*_test.cpp is a self-contained program with its own main() that
    includes that header (or inlines verbatim copies of the mod helpers, e.g.
    subrect_clear_test.cpp). The tests validate the mod's logic without needing
    the un-compilable Win32 translation unit.

    This script:
      1. Compiles every tests\*_test.cpp independently into tests\build\
      2. Runs each resulting .exe and records pass/fail (exit code 0 == pass)
      3. Writes a FULL transcript to a timestamped log in the repo root:
             test_result_run_{HH_MM_AM/PM}.log   (e.g. test_result_run_05_38_AM.log)
      4. Keeps the TERMINAL quiet and stable: no carriage-return animation.
         Output uses clear sections, aligned key/value rows, and blank lines that
         remain readable when redirected to a file. Per-test PASS detail stays in
         the transcript unless -ShowPass is used; failures are always displayed.
      5. Cleans up: removes tests\build\ so no .exe artifacts remain next to
         the source. All .cpp / .h source is left untouched.
      6. error.log: written to the repo root ONLY when something fails.
         For build/test failures it contains the failing compiler / assert
         output; for guard failures it contains ONLY the [FAIL] error lines and
         any error detail -- not the guard's full pass/summary output. Plus a
         pointer to the full transcript. On a clean PASS run the
         file is never created; if a stale one exists from a previous failure it
         is automatically deleted so its presence always means "last run failed".

    After the unit tests, the default run automatically invokes the log-layer
    guard and self-test, empty-body guard and self-test, balance check, and the
    source-invariant regression gate. No extra flags are needed.

    Options:
        -KeepBinaries        keep compiled exes in tests\build\ for debugging
        -SkipLogLayerCheck   skip the log-layer guard and its self-test
        -SkipEmptyBodyCheck  skip the empty-body guard and its self-test
        -SkipBalanceCheck    skip the delimiter balance check
        -SkipRegressionCheck    skip the source-invariant regression gate
        -SkipVersionInitCheck   skip the version / INIT consistency check
        -ShowPass               print each successful test to the terminal
        -Compiler g++        choose the C++ compiler (default: g++)
        -Std c++17           choose the language standard (default: c++17)
#>

[CmdletBinding()]
param(
    [switch]$KeepBinaries,
    [switch]$SkipBalanceCheck,
    [switch]$SkipLogLayerCheck,
    [switch]$SkipEmptyBodyCheck,
    [switch]$SkipRegressionCheck,
    [switch]$SkipVersionInitCheck,
    [switch]$ShowPass,
    [string]$Compiler = "g++",
    [string]$Std = "c++17"
)

$ErrorActionPreference = "Stop"

# Accumulates every failure detail during the run.
# Written to error.log ONLY if the overall result is FAIL; never created on PASS.
$ErrorLines = [System.Collections.Generic.List[string]]::new()

# This script lives in scripts\ ; the repo root is one level up, and tests
# live in <root>\tests. check_balance.ps1 is a sibling in this same scripts\ dir.
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$TestsDir  = Join-Path $RepoRoot "tests"
$BuildDir  = Join-Path $TestsDir "build"

# --- Timestamped transcript log in the repo root -------------------------
# Filename form: test_result_run_HH_MM_AM.log / ..._PM.log  (12-hour clock).
# Include seconds so two runs in the same minute (e.g. a nested invocation)
# never write to the SAME file and clobber each other's transcript.
$stamp   = (Get-Date).ToString("hh_mm_ss_tt").ToUpper()
$LogFile = Join-Path $RepoRoot ("test_result_run_{0}.log" -f $stamp)

# Everything the run "says" is funnelled through Write-Log. Terminal output is
# intentionally sparse; the log file gets the full, timestamp-headed transcript.
function Write-Log {
    param(
        [string]$Message = "",
        [ConsoleColor]$Color,
        [switch]$ToConsole   # when set, also echo to the terminal
    )
    Add-Content -Path $LogFile -Value $Message
    if ($ToConsole) {
        if ($PSBoundParameters.ContainsKey('Color')) {
            Write-Host $Message -ForegroundColor $Color
        } else {
            Write-Host $Message
        }
    }
}

# Stable output helpers. These write ordinary newline-delimited records instead
# of carriage-return animation, so terminal, redirected-file, and CI output all
# have the same readable structure.
function Write-Section {
    param([string]$Title)
    Write-Log ""
    Write-Log ("=== {0} ===" -f $Title) -Color Cyan -ToConsole
}

function Write-Field {
    param(
        [string]$Name,
        [string]$Value,
        [ConsoleColor]$Color
    )
    $line = "{0,-28}: {1}" -f $Name, $Value
    if ($PSBoundParameters.ContainsKey('Color')) {
        Write-Log $line -Color $Color -ToConsole
    } else {
        Write-Log $line -ToConsole
    }
}

# ---------------------------------------------------------------------------
# Read the canonical mod version from @version in taskbar-quick-pin.wh.cpp.
# Used in the FINAL RESULT header and version-drift detail row.
# ---------------------------------------------------------------------------
$ModFile = Join-Path $RepoRoot "taskbar-quick-pin.wh.cpp"
$script:ModVersion = "(unknown)"
if (Test-Path $ModFile) {
    $vMatch = [regex]::Match((Get-Content -Path $ModFile -Raw), '(?m)^//\s*@version\s+([\d]+\.[\d]+\.[\d]+)')
    if ($vMatch.Success) { $script:ModVersion = "v" + $vMatch.Groups[1].Value }
}

# Start the log and terminal with a compact, structured run header.
Set-Content -Path $LogFile -Value ("Test run: " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss"))
Write-Section "TASKBAR QUICK PIN TEST RUN"
Write-Field "Mod version" $script:ModVersion
Write-Field "Repository" $RepoRoot
Write-Field "Transcript" $LogFile

if (-not (Test-Path $TestsDir)) {
    Write-Log ("ERROR: tests directory not found at {0}" -f $TestsDir) -Color Red -ToConsole
    exit 2
}

# Verify the compiler exists.
$cc = Get-Command $Compiler -ErrorAction SilentlyContinue
if (-not $cc) {
    Write-Log ("ERROR: compiler '{0}' not found on PATH." -f $Compiler) -Color Red -ToConsole
    Write-Log "       Install MinGW-w64 (g++) or pass -Compiler <name>." -Color Red -ToConsole
    exit 2
}

# Fresh build dir.
if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Path $BuildDir | Out-Null

$tests = Get-ChildItem -Path $TestsDir -Filter *_test.cpp | Sort-Object Name
if ($tests.Count -eq 0) {
    Write-Log ("No *_test.cpp files found in {0}" -f $TestsDir) -Color Yellow -ToConsole
    exit 0
}

Write-Section "UNIT TESTS"
Write-Field "Compiler" $cc.Source
Write-Field "C++ standard" $Std
Write-Field "Test files" ("{0}" -f $tests.Count)
Write-Field "Console detail" $(if ($ShowPass) { "all PASS/FAIL rows" } else { "failures + summary" })
Write-Log ""
Write-Log ("Building and running {0} test(s). Detailed per-test output is in the transcript." -f $tests.Count) -ToConsole

$passed = 0
$failed = 0
$idx    = 0

foreach ($t in $tests) {
    $idx++
    $name = [System.IO.Path]::GetFileNameWithoutExtension($t.Name)
    $exe  = Join-Path $BuildDir ($name + ".exe")

    # Stable per-test progress is written to the transcript. The terminal stays
    # quiet by default; -ShowPass prints one ordinary line per successful test.
    Write-Log ""
    Write-Log ("[{0,2}/{1}] {2}" -f $idx, $tests.Count, $name)

    # --- Compile (headers are found via -I tests\) ---
    $compileLog = & $Compiler "-std=$Std" "-I", $TestsDir, $t.FullName "-o", $exe 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Log ("[BUILD FAIL] {0}" -f $name)
        $compileLog | ForEach-Object { Write-Log ("    $_") }
        # Surface build failures on the terminal; full compiler output is logged.
        Write-Host ("[BUILD FAIL] {0} (see transcript)" -f $name) -ForegroundColor Red
        # Capture for error.log
        $ErrorLines.Add("[BUILD FAIL] $name")
        $compileLog | ForEach-Object { $ErrorLines.Add("    $_") }
        $failed++
        continue
    }

    # --- Run ---
    $runOut = & $exe 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Log ("[PASS] {0}" -f $name)
        if ($ShowPass) {
            Write-Host ("[PASS] {0}" -f $name) -ForegroundColor Green
        }
        $passed++
    } else {
        Write-Log ("[FAIL] {0}" -f $name)
        $runOut | ForEach-Object { Write-Log ("    $_") }
        # Always surface failures on the terminal.
        Write-Host ("[FAIL] {0} (see transcript)" -f $name) -ForegroundColor Red
        $runOut | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
        # Capture for error.log
        $ErrorLines.Add("[FAIL] $name")
        $runOut | ForEach-Object { $ErrorLines.Add("    $_") }
        $failed++
    }
}

Write-Log ""
Write-Field "Passed" ("{0}" -f $passed) $(if ($failed -eq 0) { "Green" } else { "White" })
Write-Field "Failed" ("{0}" -f $failed) $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Field "Total"  ("{0}" -f $tests.Count)

# --- Cleanup: remove all build artifacts so no .exe lingers by the source ---
Write-Log ""
if (-not $KeepBinaries) {
    Remove-Item -Recurse -Force $BuildDir
    Write-Log "Cleaned build artifacts (tests\build\ removed)."
} else {
    Write-Log ("Kept build artifacts in: {0}" -f $BuildDir) -ToConsole
}

# --- Auto-invoke check_balance.ps1 (scripts\) after the test run ----------
# Runs the project's balance check so a single `run_tests.ps1` invocation does
# both. It is best-effort: a missing script only warns, and it never overrides
# a test failure. Skip it with -SkipBalanceCheck (e.g. in CI).
# --------------------------------------------------------------------------
# Post-test auto-chain: one `run_tests.ps1` invocation runs the whole gate.
#   1. check_log_layer.ps1   -- single-Wh_Log-surface lint
#   2. check_empty_body.ps1  -- empty-body control-statement lint
#   3. check_balance.ps1     -- delimiter balance sanity check
#   4. regression_check.ps1  -- source-invariant regression gate (PART 2 only;
#                               -SkipUnitTests, since WE are the unit tests)
# Each is best-effort (a missing script only warns) but a real failure flips
# $extraFailed so the overall exit code is non-zero (CI treats it as a fail).
#
# RECURSION NOTE: regression_check.ps1 calls THIS script to run the unit tests.
# To avoid an infinite loop it passes -SkipRegressionCheck (and skips the other
# sub-checks, which it runs itself). So: when invoked from the gate, we do NOT
# re-invoke the gate.
# --------------------------------------------------------------------------
$extraFailed = 0
$subCheckStatuses = [ordered]@{}

Write-Section "AUTOMATED GUARDS"
Write-Log "Each guard runs in sequence. Full output is stored in the transcript." -ToConsole
Write-Log "" -ToConsole

function Invoke-SubCheck {
    # NOTE: the param is $ExtraArgs, NOT $Args. $Args is a PowerShell AUTOMATIC
    # variable; using it as a param name does not bind reliably, which silently
    # dropped the -SkipUnitTests we pass to the regression gate and caused a
    # nested unit run (and a same-minute log-file collision). Do not rename back.
    param([string]$ScriptName, [string]$Label, [string[]]$ExtraArgs = @())
    $path = Join-Path $ScriptDir $ScriptName
    if (-not (Test-Path $path)) {
        Write-Log ("{0} not found at {1} (skipping)." -f $ScriptName, $path) -Color Yellow -ToConsole
        $script:subCheckStatuses[$Label] = "SKIPPED (missing)"
        Write-Host ("  [ SKIP ] {0,-36}  (script not found)" -f $Label) -ForegroundColor Yellow
        return
    }
    Write-Log ""
    Write-Log ("--- CHECK: {0} ---" -f $Label)
    Write-Log ("    script : {0}" -f $ScriptName)
    # Show the guard name on the terminal while it runs so the user can see progress.
    Write-Host ("  [ RUN  ] {0}" -f $Label) -ForegroundColor DarkCyan
    $out = & powershell -ExecutionPolicy Bypass -File $path @ExtraArgs 2>&1
    $code = $LASTEXITCODE
    $out | ForEach-Object { Write-Log ("    $_") }
    Write-Log ""
    if ($code -ne 0) {
        Write-Log ("    RESULT : FAIL (exit $code)")
        Write-Log ("-" * 60)
        Write-Log ("{0}: FAIL (exit {1}) -- see log." -f $Label, $code) -Color Red -ToConsole
        Write-Host ("  [ FAIL ] {0,-36}  exit $code" -f $Label) -ForegroundColor Red
        $script:subCheckStatuses[$Label] = "FAIL (exit $code)"
        # Capture for error.log -- ONLY the real error lines, not the guard's
        # full pass/summary output. Keep [FAIL] rows and any error/snippet/context
        # detail; drop [PASS] rows, PART/=== section headers and blank noise so the
        # file shows just what failed, not the whole run again.
        $script:ErrorLines.Add("[GUARD FAIL] $Label (exit $code)")
        $errOnly = $out | Where-Object {
            $ln = [string]$_
            ($ln -notmatch '\[\s*PASS\s*\]') -and
            ($ln -notmatch '^\s*PART\s') -and
            ($ln -notmatch '^\s*=+\s*$') -and
            ($ln -notmatch '^\s*===') -and
            ($ln.Trim() -ne '')
        }
        if (-not $errOnly) { $errOnly = $out }   # fallback: keep all if the filter emptied it
        $errOnly | ForEach-Object { $script:ErrorLines.Add("    $_") }
        $script:extraFailed++
    } else {
        Write-Log ("    RESULT : PASS")
        Write-Log ("-" * 60)
        Write-Log ("{0}: OK." -f $Label)
        Write-Host ("  [ PASS ] {0}" -f $Label) -ForegroundColor Green
        $script:subCheckStatuses[$Label] = "PASS"
    }
}

if (-not $SkipLogLayerCheck) {
    Invoke-SubCheck "check_log_layer.ps1" "log-layer guard"
    Invoke-SubCheck "..\tests\check_log_layer_guard_self_test.ps1" "log-layer guard self-test"
} else {
    $subCheckStatuses["log-layer guard"] = "SKIPPED"
    $subCheckStatuses["log-layer guard self-test"] = "SKIPPED"
}

if (-not $SkipEmptyBodyCheck) {
    Invoke-SubCheck "check_empty_body.ps1" "empty-body guard"
    Invoke-SubCheck "..\tests\check_empty_body_guard_self_test.ps1" "empty-body guard self-test"
} else {
    $subCheckStatuses["empty-body guard"] = "SKIPPED"
    $subCheckStatuses["empty-body guard self-test"] = "SKIPPED"
}

if (-not $SkipBalanceCheck) {
    Invoke-SubCheck "check_balance.ps1" "balance check"
} else {
    $subCheckStatuses["balance check"] = "SKIPPED"
}

if (-not $SkipVersionInitCheck) {
    Invoke-SubCheck "check_version_init.ps1" "version/INIT check"
} else {
    $subCheckStatuses["version/INIT check"] = "SKIPPED"
}

if (-not $SkipRegressionCheck) {
    # -SkipUnitTests: the gate's PART 1 is exactly this run; only run its
    # source-invariant checks (PART 2) to avoid re-running the suite.
    Invoke-SubCheck "regression_check.ps1" "regression gate" @("-SkipUnitTests")
} else {
    $subCheckStatuses["regression gate"] = "SKIPPED"
}

$overallPassed = ($failed -eq 0 -and $extraFailed -eq 0)

# ---------------------------------------------------------------------------
# Collect version-drift detail for the FINAL RESULT section.
# We re-read the mod source here (already read above for $ModVersion) to find
# what the INIT log actually reports, so the summary can show the exact drift.
# ---------------------------------------------------------------------------
$script:VersionDriftDetail = $null
if (Test-Path $ModFile) {
    $modSrc = Get-Content -Path $ModFile -Raw
    # Canonical version already in $script:ModVersion (e.g. "v2.5.3")
    $allInitVer = [regex]::Matches($modSrc, 'INIT:\s*v([\d]+\.[\d]+\.[\d]+)')
    $staleVers  = @()
    $canonical  = $script:ModVersion -replace '^v', ''
    foreach ($m in $allInitVer) {
        $found = $m.Groups[1].Value
        if ($found -ne $canonical) { $staleVers += "v$found" }
    }
    if ($staleVers.Count -gt 0) {
        $script:VersionDriftDetail = ("@version says {0}  --  INIT log still says: {1}" -f $script:ModVersion, ($staleVers -join ", "))
    }
    # Also flag if no INIT line at all
    $initForCanonical = [regex]::Matches($modSrc, "INIT:\s*v$([regex]::Escape($canonical))")
    if ($initForCanonical.Count -eq 0 -and $staleVers.Count -eq 0) {
        $script:VersionDriftDetail = ("@version says {0}  --  no matching INIT log line found" -f $script:ModVersion)
    }
}

Write-Section "FINAL RESULT"

# --- Group 1: Mod identity ---
Write-Log ""
Write-Field "Mod version" $script:ModVersion $(if ($script:VersionDriftDetail) { "Yellow" } else { "Cyan" })
if ($script:VersionDriftDetail) {
    Write-Field "  Version drift" $script:VersionDriftDetail "Red"
}

# --- Group 2: Unit tests ---
Write-Log ""
$unitColor = if ($failed -eq 0) { "Green" } else { "Red" }
Write-Field "Unit tests" ("{0} passed  /  {1} failed  /  {2} total" -f $passed, $failed, $tests.Count) $unitColor

# --- Group 3: Guards (grouped by category) ---
Write-Log ""
# Source-lint guards
foreach ($key in @("log-layer guard", "log-layer guard self-test", "empty-body guard", "empty-body guard self-test", "balance check")) {
    if ($subCheckStatuses.Contains($key)) {
        $v = $subCheckStatuses[$key]
        Write-Field ("  " + $key) $v $(if ($v -eq "PASS") { "Green" } elseif ($v -like "FAIL*") { "Red" } else { "Yellow" })
    }
}
Write-Log ""
# Version + regression guards
foreach ($key in @("version/INIT check", "regression gate")) {
    if ($subCheckStatuses.Contains($key)) {
        $v = $subCheckStatuses[$key]
        Write-Field ("  " + $key) $v $(if ($v -eq "PASS") { "Green" } elseif ($v -like "FAIL*") { "Red" } else { "Yellow" })
    }
}

# --- Group 4: Totals + transcript ---
Write-Log ""
Write-Field "Guard failures" ("{0}" -f $extraFailed) $(if ($extraFailed -eq 0) { "Green" } else { "Red" })
Write-Field "Transcript" $LogFile

# --- Verdict ---
Write-Log ""
Write-Log ("-" * 60) -ToConsole
Write-Field "OVERALL" $(if ($overallPassed) { "PASS" } else { "FAIL" }) $(if ($overallPassed) { "Green" } else { "Red" })
Write-Log ("-" * 60) -ToConsole

# --- error.log: created ONLY on failure; deleted (or never created) on PASS ---
$ErrorLogFile = Join-Path $RepoRoot "error.log"
if ($overallPassed) {
    # Clean run: remove any stale error.log from a previous failed run.
    if (Test-Path $ErrorLogFile) {
        Remove-Item -Force $ErrorLogFile
        Write-Log "Removed stale error.log (run is clean)." -ToConsole
    }
} else {
    # Something failed: write a focused error.log with full failure details.
    $header = @(
        ("error.log  --  generated by run_tests.ps1"),
        ("Run timestamp : " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")),
        ("Full transcript: " + $LogFile),
        (""),
        ("=" * 60),
        ("")
    )
    Set-Content -Path $ErrorLogFile -Value ($header + $ErrorLines)
    Write-Log ""
    Write-Log ("error.log written: {0}" -f $ErrorLogFile) -Color Red -ToConsole
}

# Non-zero exit if any unit test OR any auto-chained sub-check failed (CI).
if ($overallPassed) { exit 0 } else { exit 1 }
