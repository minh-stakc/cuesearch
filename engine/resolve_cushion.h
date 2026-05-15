#pragma once
// Mathavan-style spin-coupled cushion rebound. Coulomb friction integrated
// over the accumulated normal impulse at a contact raised to h = 7R/5
// (sin th = 2/5), so sidespin (running/reverse english) and topspin alter
// the rebound -- this is what the CP2 placeholder reflection could not do.
//
// Scope/honesty: this captures the dominant Mathavan-2010 couplings via the
// proven CP3 impulse-integration pattern with a pool-calibrated COR. The
// full 6-ODE Mathavan-2010 closed system is a flagged fidelity refinement;
// validation here is trend-only (Dr. Dave cushion data is sparse).
#include "engine/ball.h"

namespace cue {

// outwardNormal: unit horizontal normal of the struck rail (points out of
// the table). Mutates b.v, b.w.
void resolveCushion(Ball& b, const Vec3& outwardNormal);

}  // namespace cue
