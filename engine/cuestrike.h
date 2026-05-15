#pragma once
// Cue-tip strike -> initial (v, w). Reference model (Leckie&Greenspan §3 /
// jzitelli CueStrikeEvent). INTERNALIZE: be able to re-derive F, v0, w0 and
// explain the miscue limit |offset| <= R/2 at a superday.
#include "engine/ball.h"

namespace cue {

// aimDir: horizontal unit aim (xz-plane). a: side offset (right +),
// b: vertical offset (up +), both in metres on the tip face. speed: cue
// speed (m/s). mCue: cue mass (kg). Offsets are clamped to the miscue
// circle a^2+b^2 <= (R/2)^2.  Sets out.v, out.w (caller sets out.r).
void cueStrike(Ball& out, const Vec3& aimDir, double speed, double a,
               double b, double mCue = 0.54);

}  // namespace cue
