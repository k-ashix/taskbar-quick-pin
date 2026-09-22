<#
    check_empty_body.ps1  --  Guards against "empty-body control statement" syntax errors.

    Location: scripts\. Run from anywhere:

        powershell -ExecutionPolicy Bypass -File scripts\check_empty_body.ps1

    --------------------------------------------------------------------------
    Why this exists
    --------------------------------------------------------------------------
    A previous round accidentally DELETED the body of three control statements
    in taskbar-quick-pin.wh.cpp. Statements like:

        if (!result.empty())
            DoSomething();

    were reduced to just their header with the body gone:

        if (!result.empty())
        }                       <-- header immediately followed by a close brace
        if (!result.empty())
        } else {                <-- header followed by an else
        if (!result.empty())
        // comment only          <-- header followed only by a comment

    A control-statement header (`if (...)`, `for (...)`, `while (...)`, `else`,
    `else if (...)`) with NO statement and NO opening brace after it makes the
    NEXT token the statement -- and when that next token is `}`, `} else {`, or
    a comment, clang reports:  error: expected statement.  The mod translation
    unit can't be compiled standalone (it needs Windhawk/Win32 headers), so this
    lightweight source-text lint catches the recurrence in ~1 second, anywhere.

    Detection rule (source-text only):
        A "dangling" control header is a line whose trimmed text is one of
            if (...)   /   else if (...)   /   for (...)   /   while (...)
        ending in `)` with nothing after the `)` (no `{`, no `;`, no trailing
        statement), OR a bare `else`. For each such header we look at the next
        NON-BLANK line; if that line's trimmed text is:
            }   |   } else {   |   } else   |   another closing brace
            |   starts with `//`  |   starts with `/*`
        then the body was deleted -> FLAG it. A following `{` or a real
        statement is fine.

    Exit code: 0 when clean, 1 when any empty body is found, 2 only when the mod
    source file is missing. Designed to be auto-invoked by run_tests.ps1 and used
    as a CI status check.

    Options:
        -Quiet   suppress per-line detail; still print the final verdict + exit code
#>

[CmdletBinding()]
param(
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot  = Split-Path -Parent $ScriptDir
$ModFile   = Join-Path $RepoRoot "taskbar-quick-pin.wh.cpp"

if (-not (Test-Path $ModFile)) {
    Write-Host "ERROR: mod source not found at $ModFile" -ForegroundColor Red
    exit 2
}

# Read as an array of lines (1:1 with file lines for accurate line numbers).
$lines = Get-Content -Path $ModFile
$violations = 0

function Say ($m, $c) { if (-not $Quiet) { if ($c) { Write-Host $m -ForegroundColor $c } else { Write-Host $m } } }

# Is a control-statement header with NO body and NO opening brace on the line?
#   if (...) / else if (...) / for (...) / while (...)  ending in `)` with
#   nothing after, or a bare `else`. Excludes lines ending in `{`, `;`, or
#   comment-only lines.
function Test-DanglingHeader ($trimmed) {
    if ($trimmed -match '^\s*(//|/\*|\*)') { return $false }   # comment line
    if ($trimmed -match '[{;]\s*$')        { return $false }   # has brace/stmt
    if ($trimmed -match '^else\s*$')       { return $true  }   # bare else
    # if / else if / for / while whose header ends in ) with nothing after it.
    if ($trimmed -match '^(else\s+if|if|for|while)\b.*\)\s*$') { return $true }
    return $false
}

# Does the given (trimmed) next line indicate the control body was deleted?
function Test-EmptyBodyNext ($trimmed) {
    if ($trimmed -match '^(//|/\*)') { return $true }          # comment only
    if ($trimmed -match '^\}\s*else\s*\{?\s*$') { return $true }  # } else / } else {
    if ($trimmed -match '^\}') { return $true }                # closing brace
    return $false
}

Say "=== empty-body control-statement guard ===" White
Say "" $null

for ($i = 0; $i -lt $lines.Count; $i++) {
    $trimmed = $lines[$i].Trim()
    if (-not (Test-DanglingHeader $trimmed)) { continue }

    # Find the next NON-BLANK line.
    $j = $i + 1
    while ($j -lt $lines.Count -and $lines[$j].Trim() -eq "") { $j++ }
    if ($j -ge $lines.Count) { continue }

    $nextTrim = $lines[$j].Trim()
    if (Test-EmptyBodyNext $nextTrim) {
        $violations++
        if (-not $Quiet) {
            Write-Host ("  [FAIL] line {0}: {1}  ->  {2}" -f ($i + 1), $trimmed, $nextTrim) -ForegroundColor Red
        }
    }
}

Write-Host ""
if ($violations -eq 0) {
    Write-Host "EMPTY-BODY GUARD: PASS (no dangling control headers)" -ForegroundColor Green
    exit 0
} else {
    Write-Host ("EMPTY-BODY GUARD: FAIL ({0} empty-body control statement(s)) -- a deleted body would make clang report 'expected statement'" -f $violations) -ForegroundColor Red
    exit 1
}
