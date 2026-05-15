#pragma once
// THE NOVELTY. Single-shot solver: ghost-ball candidate generation -> Monte
// Carlo execution-noise rollouts over the physics forward model -> P(legal
// pot). This is search/optimisation under a stochastic forward model -- the
// quant-relevant core no open-source pool engine provides.
#include <random>
#include <vector>

#include "core/constants.h"
#include "engine/game.h"
#include "engine/world.h"

namespace cue {

struct ShotParam {
    Vec3 aim{1, 0, 0};   // unit horizontal aim for the cue
    double speed = 2.0;  // cue speed (m/s)
    double a = 0.0;      // side tip offset (m)
    double b = 0.0;      // vertical tip offset (m)
};

// Direct ghost-ball shot, object-ball bank (off one rail into a pocket), or
// cue kick (cue off one rail to reach the target -- snooker escape).
// Combo: legal ball -> 9 -> pocket. Carom: cue legally hits the legal ball
// then caroms into the 9. Both are instant wins (9 down, lowest hit first)
// and are only generated when the 9 sits near a pocket.
enum class ShotKind { Direct, Bank, Kick, Safety, Combo, Carom };

struct ShotEval {
    ShotParam shot;
    int targetId = -1;   // object ball this shot intends to pot
    int pocket = -1;     // pocket index 0..5
    ShotKind kind = ShotKind::Direct;
    int rail = -1;       // rail used by a Bank/Kick (0:xMin 1:xMax 2:zMin 3:zMax)
    double pPot = 0.0;   // MC estimate of P(legal pot of target)
};

// Ghost-ball candidates: pot the legal target into each feasible pocket,
// across a small speed/spin grid. All respect the miscue clip a^2+b^2<=(R/2)^2.
std::vector<ShotEval> candidateShots(const World& w);

// P(legal pot of e.targetId) under Gaussian aim/speed execution noise.
// Rollouts are deterministically seeded per-index and run in parallel, so
// the result is bitwise-independent of thread count (deterministic
// parallelism).
double evaluate(const World& w, const ShotEval& e, int nRollouts,
                unsigned baseSeed, double aimSigmaRad = k::AIM_SIGMA,
                double speedRelSigma = k::SPEED_SIGMA);

// Soft legal-contact shots at the legal target, no pocket intent. Safety
// is NOT special-cased in the objective -- these just become available and
// the win-probability solver picks them when they beat shooting.
std::vector<ShotEval> safetyCandidates(const World& w);

// Best single shot for the current legal target. Deterministic given seed.
ShotEval bestShot(const World& w, int nRollouts = 64, unsigned seed = 1);

}  // namespace cue
