// Behavioral tests for the sub-rect clear + premultiply helpers (item 8).
//
// These mirror the EXACT bodies of ClearSurfaceRect / PremultiplyAlphaRect in
// taskbar-quick-pin.wh.cpp (kept in sync deliberately -- the mod cannot be
// compiled standalone because it needs the Windhawk/Win32 headers). They prove
// the two properties that matter for correctness:
//   1. Pixels OUTSIDE the content sub-rect are never touched (so the fixed,
//      re-presented layered surface keeps whatever was there -- transparent).
//   2. Premultiply is applied to exactly the sub-rect, matching the old
//      whole-surface premultiply for the drawn region.

#include <cstring>
#include <cstdio>

using BYTE = unsigned char;

// ---- verbatim copies of the mod helpers (stride = surfW, BGRA) -------------
static inline void ClearSurfaceRect(BYTE* bits, int surfW, int rectW, int rectH) {
    if (rectW <= 0 || rectH <= 0) return;
    if (rectW == surfW) {
        memset(bits, 0, (size_t)surfW * rectH * 4);
        return;
    }
    for (int y = 0; y < rectH; ++y)
        memset(bits + (size_t)y * surfW * 4, 0, (size_t)rectW * 4);
}

static inline void PremultiplyAlphaRect(BYTE* bits, int surfW, int rectW, int rectH) {
    if (rectW <= 0 || rectH <= 0) return;
    for (int y = 0; y < rectH; ++y) {
        BYTE* row = bits + (size_t)y * surfW * 4;
        for (int x = 0; x < rectW; ++x) {
            BYTE* px = row + (size_t)x * 4;
            BYTE  a  = px[3];
            px[0] = (BYTE)(px[0] * a / 255);
            px[1] = (BYTE)(px[1] * a / 255);
            px[2] = (BYTE)(px[2] * a / 255);
        }
    }
}

static int g_failures = 0;
#define CHECK(cond)                                                            \
    do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

static const int W = 32, H = 32;   // fixed surface
static BYTE surf[W * H * 4];

static void fill(BYTE v) { memset(surf, v, sizeof(surf)); }
static BYTE* at(int x, int y) { return surf + ((size_t)y * W + x) * 4; }

static void test_clear_touches_only_subrect() {
    fill(0xAB);                        // whole surface "dirty"
    ClearSurfaceRect(surf, W, /*rectW*/8, /*rectH*/4);

    // Inside the sub-rect -> zeroed.
    CHECK(at(0, 0)[0] == 0 && at(7, 3)[3] == 0);
    // Just outside on X (col 8) -> untouched.
    CHECK(at(8, 0)[0] == 0xAB);
    // Just outside on Y (row 4) -> untouched.
    CHECK(at(0, 4)[0] == 0xAB);
    // Far corner -> untouched.
    CHECK(at(W - 1, H - 1)[0] == 0xAB);
}

static void test_premultiply_only_subrect() {
    fill(0x00);
    // One opaque white pixel inside the rect, one identical outside it.
    BYTE* in  = at(1, 1);  in[0] = in[1] = in[2] = 200; in[3] = 128;
    BYTE* out = at(20, 1); out[0] = out[1] = out[2] = 200; out[3] = 128;

    PremultiplyAlphaRect(surf, W, /*rectW*/8, /*rectH*/8);

    // Inside: 200 * 128 / 255 == 100, alpha unchanged.
    CHECK(in[0] == 100 && in[3] == 128);
    // Outside the rect: left exactly as it was (straight, not premultiplied).
    CHECK(out[0] == 200 && out[3] == 128);
}

static void test_full_width_fast_path_matches() {
    // rectW == surfW should behave the same as the per-row loop.
    fill(0x7F);
    ClearSurfaceRect(surf, W, /*rectW*/W, /*rectH*/3);
    CHECK(at(0, 0)[0] == 0 && at(W - 1, 2)[0] == 0);   // first 3 rows cleared
    CHECK(at(0, 3)[0] == 0x7F);                         // row 3 untouched
}

int main() {
    test_clear_touches_only_subrect();
    test_premultiply_only_subrect();
    test_full_width_fast_path_matches();
    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
