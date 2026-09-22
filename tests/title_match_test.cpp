// Unit tests for L2 taskbar title matching: when the real windhawk.exe window
// is open and in the FOREGROUND, clicking a taskbar button must still resolve
// to the button's own app -- not to windhawk.exe -- and must not MISS.
//
// Field log this pins:
//   RESOLVER MISS: all layers failed near (817,1390)
//   RESOLVER MISS: all layers failed near (1622,1414)
// "mostly when the actual windhawk exe is open and in the foreground it skips
//  most of the action".
//
// Root cause: TitleMatchEnumProc scored substring matches (60/40) plus a +20
// WS_EX_APPWINDOW bonus on a flat scale, so the foreground Windhawk window --
// whose title embeds the mod moniker / target exe name and is an app window --
// could out-score the button's EXACT-title window. The fix makes an exact match
// its own tier that no substring + style bonus can beat.

#include "title_match.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Replica of the ORIGINAL (buggy) mod scoring, used only to prove these tests
// actually catch the reported regression: exact=100, title-contains-needle=60,
// needle-contains-title=40, +20 app window, -30 tool window.
static int OldScore(const std::wstring& title, const std::wstring& needle,
                    bool isApp, bool isTool) {
    int s;
    if (Tm_IEquals(title, needle))         s = 100;
    else if (Tm_IContains(title, needle))  s = 60;
    else if (Tm_IContains(needle, title))  s = 40;
    else return -1;
    if (isApp)  s += 20;
    if (isTool) s -= 30;
    return s;
}
static std::wstring OldPickBestExePath(const std::wstring& needle,
                                       const std::vector<TmCandidate>& ws) {
    int best = -1; std::wstring bestExe;
    for (const auto& w : ws) {
        int s = OldScore(w.title, needle, w.isAppWindow, w.isToolWindow);
        if (s > best) { best = s; bestExe = w.exePath; }
    }
    return best >= 0 ? bestExe : std::wstring();
}

// ---- THE BUG: foreground windhawk out-scores the exact-title target ---------

static void test_foreground_windhawk_does_not_hijack_exact_match() {
    // The clicked taskbar button's UIA Name == the target window's title.
    const std::wstring needle = L"Antigravity IDE";

    std::vector<TmCandidate> windows = {
        // Real windhawk editor, foreground, app window, title embeds the needle
        // (Windhawk shows the mod / target being edited).
        { L"Windhawk - Antigravity IDE (taskbar-quick-pin)",
          L"C:\\Program Files\\Windhawk\\windhawk.exe",
          /*app=*/true, /*tool=*/false },
        // The actual target window: exact title match, ordinary app window.
        { L"Antigravity IDE",
          L"C:\\AI_Agents\\Antigravity_IDE\\Antigravity IDE.exe",
          /*app=*/true, /*tool=*/false },
    };

    // Demonstrate the regression under the OLD scorer: windhawk (60+20=80) beats
    // the exact target (100)? No -- 100 wins. The real hijack is the reverse
    // substring case below, but keep this as a guard that exact still wins here.
    std::wstring fixed = Tm_PickBestExePath(needle, windows);
    CHECK(fixed == L"C:\\AI_Agents\\Antigravity_IDE\\Antigravity IDE.exe");
}

static void test_reverse_substring_foreground_windhawk_hijack() {
    // The dangerous case from the field: the taskbar button Name is LONGER than
    // the windhawk window title fragment, so "needle CONTAINS window title"
    // (old score 40) fires for windhawk, AND windhawk is an app window (+20=60),
    // while the true target matches exactly (100). Old scorer: 100 wins -> ok.
    //
    // The true hijack: the target's own window is briefly a TOOL window (common
    // for splash/child hosts) so its exact match is penalised (-30 => 70), while
    // foreground windhawk's substring app-window match (60+20=80) WINS under the
    // OLD flat scale -> resolver returns windhawk.exe or discards -> MISS.
    const std::wstring needle = L"Antigravity IDE";

    std::vector<TmCandidate> windows = {
        { L"Antigravity IDE - Windhawk",                       // contains needle
          L"C:\\Program Files\\Windhawk\\windhawk.exe",
          /*app=*/true, /*tool=*/false },                      // OLD: 60+20 = 80
        { L"Antigravity IDE",                                  // exact target
          L"C:\\AI_Agents\\Antigravity_IDE\\Antigravity IDE.exe",
          /*app=*/false, /*tool=*/true },                      // OLD: 100-30 = 70
    };

    // Prove the bug is real under the OLD scoring: it picks windhawk.
    CHECK(OldPickBestExePath(needle, windows) ==
          L"C:\\Program Files\\Windhawk\\windhawk.exe");

    // Fixed scoring: EXACT tier (3000) dominates the substring tier (<=2000+bonus)
    // no matter the style penalty, so the true target wins.
    CHECK(Tm_PickBestExePath(needle, windows) ==
          L"C:\\AI_Agents\\Antigravity_IDE\\Antigravity IDE.exe");
}

static void test_no_miss_when_only_exact_match_is_tool_window() {
    // Even if the only exact-title window is a tool window and a foreground app
    // window merely shares a substring, we must still resolve (no MISS) to the
    // exact target.
    const std::wstring needle = L"Notepad";
    std::vector<TmCandidate> windows = {
        { L"Notepad helper overlay", L"C:\\Program Files\\Windhawk\\windhawk.exe",
          true, false },
        { L"Notepad", L"C:\\Windows\\System32\\notepad.exe", false, true },
    };
    std::wstring exe = Tm_PickBestExePath(needle, windows);
    CHECK(!exe.empty());
    CHECK(exe == L"C:\\Windows\\System32\\notepad.exe");
}

// ---- Tier ordering is absolute (style can only break intra-tier ties) -------

static void test_exact_beats_any_substring_regardless_of_style() {
    CHECK(Tm_Score(L"Foo", L"Foo", /*app=*/false, /*tool=*/true) >
          Tm_Score(L"Foo Bar", L"Foo", /*app=*/true, /*tool=*/false));
}

static void test_style_only_breaks_ties_within_tier() {
    // Two substring (needle-in-title) matches: the app window wins over the
    // tool window, but neither can cross into the exact tier.
    int app  = Tm_Score(L"Foo Bar", L"Foo", true,  false);
    int tool = Tm_Score(L"Foo Baz", L"Foo", false, true);
    CHECK(app > tool);
    CHECK(app < Tm_Score(L"Foo", L"Foo", false, true)); // still below exact
}

// ---- Degenerate guards ------------------------------------------------------

static void test_empty_needle_matches_nothing() {
    std::vector<TmCandidate> windows = {
        { L"Anything", L"C:\\a.exe", true, false },
    };
    CHECK(Tm_PickBestExePath(L"", windows).empty());
}

static void test_no_match_returns_empty() {
    std::vector<TmCandidate> windows = {
        { L"Totally Unrelated", L"C:\\a.exe", true, false },
    };
    CHECK(Tm_PickBestExePath(L"Antigravity IDE", windows).empty());
}

int main() {
    test_foreground_windhawk_does_not_hijack_exact_match();
    test_reverse_substring_foreground_windhawk_hijack();
    test_no_miss_when_only_exact_match_is_tool_window();
    test_exact_beats_any_substring_regardless_of_style();
    test_style_only_breaks_ties_within_tier();
    test_empty_needle_matches_nothing();
    test_no_match_returns_empty();

    if (g_failures == 0) {
        std::printf("title_match: ALL PASS\n");
        return 0;
    }
    std::printf("title_match: %d FAILURE(S)\n", g_failures);
    return 1;
}
