// Pure, Win32-free decision logic for the cinematic lock/unlock glow flash.
//
// Mirrored from taskbar-quick-pin.wh.cpp so the glow's colour ramp, alpha
// envelope, and seal-vs-ripple geometry can be unit-tested in isolation, with
// no Win32 dependency (the actual on-screen bloom is a layered-window effect
// that can only be eyeballed on a live Windhawk desktop).
//
// Behaviour being pinned: when the icon lock toggles (triple-tap L), or a
// locked gesture-unpin is refused, the dock plays a brief (~750 ms) flash.
//   LOCK   -> warm GOLD "seal-in" bloom that CONVERGES onto the dock edge.
//   UNLOCK -> cool GREEN "release" ripple that EXPANDS outward from the edge.
// The kind is chosen from the (already-updated) locked state at trigger time,
// so no keyboard-gesture call site needs to know about the visual.

#ifndef TASKBAR_QUICK_PIN_LOCK_GLOW_H
#define TASKBAR_QUICK_PIN_LOCK_GLOW_H

#include <cmath>   // sqrtf for the rounded-rect signed-distance

enum LockGlowKind { LOCKGLOW_SEAL, LOCKGLOW_RELEASE, LOCKGLOW_LIMIT };

// TriggerLockGlow() runs AFTER the caller flips the lock flag, so the NEW locked
// state selects the effect: locked => gold seal-in, unlocked => green release.
// (A blocked triple-tap-U fires while still locked, so it also shows the gold
// seal -- the intended "it's locked" feedback.)
static inline LockGlowKind LockGlowKindFromLocked(bool locked) {
    return locked ? LOCKGLOW_SEAL : LOCKGLOW_RELEASE;
}

// Flash alpha envelope over normalized time t in [0,1]: a fast smoothstep rise
// to a strong peak early (~22% in), then a gentle quadratic ease-out to 0. It
// is 0 at both ends so the layer fades cleanly in and fully out. Returns 0..1.
static inline float LockGlowAlpha01(float t) {
    if (t <= 0.0f || t >= 1.0f) return 0.0f;
    const float peak = 0.22f;                  // brightest point of the flash
    if (t < peak) {
        float r = t / peak;                    // 0..1 rising
        return r * r * (3.0f - 2.0f * r);      // smoothstep in
    }
    float f = (t - peak) / (1.0f - peak);      // 0..1 falling
    float e = 1.0f - f;
    return e * e;                              // quadratic ease-out
}

// Plain 0..255 RGB triple (premultiplication by alpha happens in the renderer).
struct LockGlowRGB { int r, g, b; };

// Colour ramp per kind, brightening toward the peak (p in [0,1], typically the
// current alpha envelope value). SEAL = amber -> bright gold; RELEASE = deep
// green -> bright green.
static inline LockGlowRGB LockGlowColor(LockGlowKind kind, float p) {
    if (p < 0.0f) p = 0.0f; else if (p > 1.0f) p = 1.0f;
    if (kind == LOCKGLOW_SEAL) {
        // amber (184,134,11) -> bright gold (255,205,90)
        return LockGlowRGB{
            (int)(184 + (255 - 184) * p),
            (int)(134 + (205 - 134) * p),
            (int)(11  + (90  - 11 ) * p)
        };
    }
    if (kind == LOCKGLOW_LIMIT) {
        // LIMIT: deep red (120,20,20) -> bright red (255,70,70). Dock full / pin limit.
        return LockGlowRGB{
            (int)(120 + (255 - 120) * p),
            (int)(20  + (70  - 20 ) * p),
            (int)(20  + (70  - 20 ) * p)
        };
    }
    // RELEASE: deep green (20,120,70) -> bright green (80,235,140)
    return LockGlowRGB{
        (int)(20  + (80  - 20 ) * p),
        (int)(120 + (235 - 120) * p),
        (int)(70  + (140 - 70 ) * p)
    };
}

// ---------------------------------------------------------------------------
// Confinement + render-mode logic (added for the "inside-the-dock only" rework)
//
// The glow must stay INSIDE the actual dock rectangle -- never spill outside it
// and never draw a fabricated outline. Two render modes, chosen by a single
// user setting (default OFF):
//   * SWEEP (setting ON)  -- a loading-style bar that fills the whole dock
//                            LEFT -> RIGHT, then vanishes.
//   * EDGE  (setting OFF) -- a brief highlight on the dock's OWN rectangle edge
//                            (the real geometry), then calm. Default.
// ---------------------------------------------------------------------------

enum LockGlowMode { LOCKGLOW_MODE_EDGE, LOCKGLOW_MODE_SWEEP };

// The user toggle "enableLockAnimation" is OFF by default. OFF => edge-only
// highlight; ON => full left-to-right sweep across the dock.
static inline LockGlowMode LockGlowModeFromSetting(bool animationEnabled) {
    return animationEnabled ? LOCKGLOW_MODE_SWEEP : LOCKGLOW_MODE_EDGE;
}

// Left -> right sweep front position, in [0,1] across the dock's own width, as
// the flash advances (t in [0,1]). 0 = left dock edge, 1 = right dock edge.
// Monotonic non-decreasing so the fill only ever advances rightward (a
// smoothstep gives it the ease-in/ease-out "loading" feel).
static inline float LockGlowSweepX01(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);   // smoothstep 0..1
}

// Softer, calmer peak alpha ceiling applied on top of the envelope so the flash
// reads as a gentle cinematic glow rather than a hard blink. In [0,1].
static const float LOCKGLOW_PEAK_ALPHA = 0.55f;

// Signed distance from pixel (px,py) to the ROUNDED dock rectangle border
// [dl,dt)-(dr,db) with corner radius cornerR (0 = square). Negative inside,
// 0 on the rounded edge, positive outside. Lets the glow follow the dock's
// real rounded corners so it never pokes past them.
static inline float LockGlowRoundRectSD(float px, float py,
                                        float dl, float dt, float dr, float db,
                                        float cornerR) {
    float halfx = (dr - dl) * 0.5f, halfy = (db - dt) * 0.5f;
    float ccx   = (dl + dr) * 0.5f, ccy   = (dt + db) * 0.5f;
    if (cornerR > halfx) cornerR = halfx;
    if (cornerR > halfy) cornerR = halfy;
    float qx = (px - ccx); qx = (qx < 0 ? -qx : qx) - (halfx - cornerR);
    float qy = (py - ccy); qy = (qy < 0 ? -qy : qy) - (halfy - cornerR);
    float ax = qx > 0.f ? qx : 0.f, ay = qy > 0.f ? qy : 0.f;
    float outside = sqrtf(ax * ax + ay * ay);
    float mmax = qx > qy ? qx : qy;
    float inside = mmax < 0.f ? mmax : 0.f;
    return outside + inside - cornerR;
}

// Confinement predicate: is buffer pixel (px,py) allowed to be lit, given the
// dock's interior box [dl,dt)-(dr,db) and its corner radius cornerR in the SAME
// buffer coordinates? Nothing outside the ROUNDED rect is ever lit (the glow
// follows the dock's real corners; no fabricated outline).
//   SWEEP: inside the rounded rect AND at/behind the sweep front.
//   EDGE : within `edgeBand` px of the rounded INNER border only -- highlights
//          the dock's real edge without painting outside it.
// cornerR defaults to 0 (square) so square-rect callers/tests are unaffected.
static inline bool LockGlowPixelAllowed(LockGlowMode mode,
                                        float px, float py,
                                        float dl, float dt, float dr, float db,
                                        float sweepX01, float edgeBand,
                                        float cornerR = 0.0f) {
    if (px < dl || px >= dr || py < dt || py >= db) return false;  // bounding box
    float sd = LockGlowRoundRectSD(px, py, dl, dt, dr, db, cornerR);
    if (sd > -0.5f) return false;          // outside / on the rounded corner -> clip
    if (mode == LOCKGLOW_MODE_SWEEP) {
        float frontX = dl + (dr - dl) * sweepX01;
        return px <= frontX;
    }
    float depth = -sd;                     // >0 inside; 0 at the edge
    return depth < edgeBand;
}

// ---------------------------------------------------------------------------
// Remover / lifecycle guard (mirrored 1:1 in taskbar-quick-pin.wh.cpp)
//
// The glow rides on the dock's REAL rectangle (g_cachedDockRect). If the dock
// stops showing while a flash is mid-flight, the flash must be torn down at
// once (window hidden + timer zeroed) or it is left painting over empty space
// where the dock used to be. The dock hides for THREE hard reasons, mirroring
// RepositionOverlay's early-return branches: a fullscreen app / snip overlay,
// an unsupported (left-aligned) layout, or a zero-width boot race.
// ---------------------------------------------------------------------------

// Is the dock overlay hidden this frame for a reason that must also hide the
// glow? (dockLocalW <= 0 means no valid geometry yet / degenerate.)
static inline bool DockHiddenForGlow(bool fullscreenActive, bool layoutUnsupported,
                                     int dockLocalW) {
    return fullscreenActive || layoutUnsupported || dockLocalW <= 0;
}

// Should the in-flight glow be removed this frame? Nothing to remove when no
// flash is active. Otherwise remove immediately if the dock is hidden (the
// stuck-glow fix), or once the flash has run its full duration.
static inline bool LockGlowShouldTeardown(bool glowActive, bool dockHidden,
                                          unsigned elapsedMs, unsigned durationMs) {
    if (!glowActive) return false;   // idle: nothing on screen to remove
    if (dockHidden)  return true;    // dock gone -> glow must go with it, now
    return elapsedMs >= durationMs;  // flash finished normally
}

#endif  // TASKBAR_QUICK_PIN_LOCK_GLOW_H
