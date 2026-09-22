// Pure decision: given a UIA taskbar-button Name (the needle) and the set of
// visible top-level windows, which window's process is the real drag/press
// source?
//
// Shared (by mirroring, like the other tests/*.h) between the Windhawk mod
// (taskbar-quick-pin.wh.cpp, Resolver_Layer2_TaskbarIntelligence /
// TitleMatchEnumProc) and its unit tests (title_match_test.cpp), with no Win32
// deps.
//
// Bug this pins ("skips most actions when the real windhawk.exe is open and in
// the foreground"):
//
//   14:00:15 RESOLVER MISS: all layers failed near (817,1390)
//   14:00:18 RESOLVER MISS: all layers failed near (1622,1414)
//
// When a taskbar button is clicked, L2 reads its UIA Name (== the target app's
// window title) and then EnumWindows-scores every visible top-level window:
//     exact title            -> 100
//     window title CONTAINS needle -> 60
//     needle CONTAINS window title -> 40
//     + WS_EX_APPWINDOW      -> +20
//     - WS_EX_TOOLWINDOW     -> -30
//
// The Windhawk editor/main window is a real, visible, WS_EX_APPWINDOW top-level
// window whose title embeds the moniker of the mod being edited
// ("taskbar-quick-pin") and often the target exe name/path. So when the clicked
// button's Name is a substring of the Windhawk window title (or vice-versa),
// the substring rules (60/40) plus the +20 app-window bonus let windhawk.exe
// OUT-SCORE the exact-match target. The resolver then either returns
// windhawk.exe (wrong) or, because windhawk is treated as its own app and the
// real target got discarded, clears to "" -> RESOLVER MISS on the taskbar path
// (which has no L1/L3 fallback). The stronger the foreground competitor, the
// more often the action is skipped.
//
// Fix contract:
//   1. An EXACT (case-insensitive) title match must always beat any substring
//      match, regardless of window styles. Style bonuses may only order
//      candidates WITHIN the same match tier, never promote a weaker tier above
//      a stronger one.
//   2. A mere substring collision with an unrelated foreground window must not
//      out-score the button's exact-title window.

#ifndef TASKBAR_QUICK_PIN_TITLE_MATCH_H
#define TASKBAR_QUICK_PIN_TITLE_MATCH_H

#include <string>
#include <vector>

// --- case-insensitive helpers (no Win32) ------------------------------------
static inline std::wstring Tm_Lower(const std::wstring& s) {
    std::wstring o = s;
    for (wchar_t& c : o)
        if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
    return o;
}
static inline bool Tm_IEquals(const std::wstring& a, const std::wstring& b) {
    return Tm_Lower(a) == Tm_Lower(b);
}
static inline bool Tm_IContains(const std::wstring& hay, const std::wstring& needle) {
    if (needle.empty()) return false;
    return Tm_Lower(hay).find(Tm_Lower(needle)) != std::wstring::npos;
}

// Match tiers, ordered so that a higher tier ALWAYS wins over a lower one,
// independent of style bonuses. This is the core of the fix: an exact match is
// its own tier above every substring match.
enum TmTier {
    TM_TIER_NONE     = 0,
    TM_TIER_NEEDLE_IN_TITLE = 1,  // needle contained in window title (weakest substring)
    TM_TIER_TITLE_IN_NEEDLE = 2,  // window title contained in needle
    TM_TIER_EXACT    = 3          // exact (case-insensitive) title == needle
};

// Style bonus is a SMALL intra-tier tie-break only. It is intentionally smaller
// than the gap between tiers so it can never promote a lower tier.
static inline int Tm_StyleBonus(bool isAppWindow, bool isToolWindow) {
    int b = 0;
    if (isAppWindow)  b += 2;
    if (isToolWindow) b -= 3;
    return b;
}

struct TmCandidate {
    std::wstring title;
    std::wstring exePath;   // process path behind this window
    bool         isAppWindow;
    bool         isToolWindow;
};

// Returns the tier for a single (needle, title) pair.
static inline TmTier Tm_ScoreTier(const std::wstring& title, const std::wstring& needle) {
    if (title.empty() || needle.empty()) return TM_TIER_NONE;
    if (Tm_IEquals(title, needle))       return TM_TIER_EXACT;
    if (Tm_IContains(title, needle))     return TM_TIER_NEEDLE_IN_TITLE;
    if (Tm_IContains(needle, title))     return TM_TIER_TITLE_IN_NEEDLE;
    return TM_TIER_NONE;
}

// Combined score: tier dominates (scaled far above any style bonus), style
// only breaks ties within a tier.
static inline int Tm_Score(const std::wstring& title, const std::wstring& needle,
                           bool isAppWindow, bool isToolWindow) {
    TmTier t = Tm_ScoreTier(title, needle);
    if (t == TM_TIER_NONE) return -1;
    return (int)t * 1000 + Tm_StyleBonus(isAppWindow, isToolWindow);
}

// Pick the exePath of the best-matching window for the given needle. Returns
// empty string when nothing matches. Iterates in the given order; ties keep the
// first seen (stable), matching EnumWindows' "score > best" semantics.
static inline std::wstring Tm_PickBestExePath(const std::wstring& needle,
                                              const std::vector<TmCandidate>& windows) {
    int best = -1;
    std::wstring bestExe;
    for (const TmCandidate& w : windows) {
        int s = Tm_Score(w.title, needle, w.isAppWindow, w.isToolWindow);
        if (s > best) { best = s; bestExe = w.exePath; }
    }
    return best >= 0 ? bestExe : std::wstring();
}

#endif  // TASKBAR_QUICK_PIN_TITLE_MATCH_H
