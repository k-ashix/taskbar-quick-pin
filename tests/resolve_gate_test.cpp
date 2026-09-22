// Unit tests for the full 5-stage lazy-resolution gate (ShouldResolve /
// ResolveCount).
//
// The headline guarantee of the refactor: the expensive resolver
// (ResolveDragSourceAtPoint + Explorer-workspace UIA probe + icon extraction)
// runs AT MOST ONCE per genuine drag-to-pin, and only when every stage passes.
// Everything else -- hovers, clicks, dock-icon reorders/unpins, and windows
// dragged across the desktop -- resolves ZERO times.

#include "resolve_gate.h"
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// A fully-passing external drag-to-pin: the one case that earns a resolve.
static DragGesture goodPin() {
    DragGesture g;
    g.fromDock          = false;
    g.exceededThreshold = true;
    g.showedDockIntent  = true;
    g.candidateLive     = true;
    g.candidateSystem   = false;
    return g;
}

static void test_genuine_drag_to_pin_resolves_exactly_once() {
    DragGesture g = goodPin();
    CHECK(ShouldResolve(g) == true);
    CHECK(ResolveCount(g) == 1);   // exactly once, never more
}

static void test_dock_icon_drag_never_resolves() {
    // Stage 1 internal path (reorder / rope-unpin): cheap, structurally cannot
    // reach the candidate gate even though it's a real, dock-intent drag.
    DragGesture g = goodPin();
    g.fromDock = true;
    CHECK(ShouldResolve(g) == false);
    CHECK(ResolveCount(g) == 0);
}

static void test_click_not_drag_never_resolves() {
    DragGesture g = goodPin();
    g.exceededThreshold = false;   // never crossed SM_CXDRAG/SM_CYDRAG
    CHECK(ResolveCount(g) == 0);
}

static void test_drag_away_from_dock_never_resolves() {
    // Window dragged across the desktop and dropped -- no dock intent ever.
    DragGesture g = goodPin();
    g.showedDockIntent = false;
    CHECK(ResolveCount(g) == 0);
}

static void test_stale_candidate_never_resolves() {
    DragGesture g = goodPin();
    g.candidateLive = false;       // window gone by drop time
    CHECK(ResolveCount(g) == 0);
}

static void test_system_surface_never_resolves() {
    DragGesture g = goodPin();
    g.candidateSystem = true;      // taskbar / Start / shell
    CHECK(ResolveCount(g) == 0);
}

static void test_resolve_count_is_never_more_than_one() {
    // Exhaustively sweep every combination of the five booleans; the resolver
    // must fire 0 or 1 times -- never twice -- for any single gesture, and
    // exactly the fully-passing external case yields 1.
    int onces = 0;
    for (int mask = 0; mask < 32; ++mask) {
        DragGesture g;
        g.fromDock          = (mask & 1)  != 0;
        g.exceededThreshold = (mask & 2)  != 0;
        g.showedDockIntent  = (mask & 4)  != 0;
        g.candidateLive     = (mask & 8)  != 0;
        g.candidateSystem   = (mask & 16) != 0;

        int n = ResolveCount(g);
        CHECK(n == 0 || n == 1);   // at most once, always
        if (n == 1) {
            ++onces;
            // The only shape that resolves: external, real drag, dock intent,
            // live, non-system.
            CHECK(!g.fromDock && g.exceededThreshold && g.showedDockIntent &&
                  g.candidateLive && !g.candidateSystem);
        }
    }
    // Exactly one of the 32 combinations earns a resolve.
    CHECK(onces == 1);
}

int main() {
    test_genuine_drag_to_pin_resolves_exactly_once();
    test_dock_icon_drag_never_resolves();
    test_click_not_drag_never_resolves();
    test_drag_away_from_dock_never_resolves();
    test_stale_candidate_never_resolves();
    test_system_surface_never_resolves();
    test_resolve_count_is_never_more_than_one();

    if (g_failures == 0) {
        std::printf("resolve_gate: ALL PASS\n");
        return 0;
    }
    std::printf("resolve_gate: %d FAILURE(S)\n", g_failures);
    return 1;
}
