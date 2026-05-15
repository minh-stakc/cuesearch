#pragma once
// Cue-tip strike -> initial (v, w). Reference model (Leckie&Greenspan §3 /
// jzitelli CueStrikeEvent). INTERNALIZE: be able to re-derive F, v0, w0 and
// explain the miscue limit |offset| <= R/2 at a superday.
#include "engine/ball.h"

namespace cue {

// aimDir: horizontal unit aim (xz-plane). a: side offset (right +),
// b: vertical offset (up +), both in metres on the tip face. speed: cue
// speed (m/s). mCue: cue mass (kg). elevationRad tilts the spin axis
// (masse/swerve; no airborne launch -- jump is roadmap). mbOverMe is the
// ball/endmass ratio for Shepard squirt (~20 high-deflection .. ~100 low).
// Offsets clamped to the miscue circle a^2+b^2 <= (R/2)^2. Applies squirt:
// the cue ball departs deflected from aimDir, opposite the english side.
// Sets out.v, out.w (caller sets out.r).
void cueStrike(Ball& out, const Vec3& aimDir, double speed, double a,
               double b, double mCue = 0.54, double elevationRad = 0.0,
               double mbOverMe = 40.0);

}  // namespace cue
