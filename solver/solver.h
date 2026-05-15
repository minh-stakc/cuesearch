#pragma once
// THE NOVELTY. Single-shot solver: ghost-ball candidate generation -> Monte
// Carlo execution-noise rollouts over the physics forward model -> P(legal
// pot). This is search/optimisation under a stochastic forward model -- the
// quant-relevant core no open-source pool engine provides.
#include <random>
#include <vector>

#include "engine/game.h"
#include "engine/world.h"

namespace cue {

struct ShotParam {
    Vec3 aim{1, 0, 0};   // unit horizontal aim for the cue
    double speed = 2.0;  // cue speed (m/s)
    double a = 0.0;      // side tip offset (m)
    double b = 0.0;      // vertical tip offset (m)
};

struct ShotEval {
    ShotParam shot;
    int targetId = -1;   // object ball this shot intends to pot
    int pocket = -1;     // pocket index 0..5
    double pPot = 0.0;   // MC estimate of P(legal pot of target)
};

// Ghost-ball candidates: pot the legal target into each feasible pocket,
// across a small speed/spin grid. All respect the miscue clip a^2+b^2<=(R/2)^2.
std::vector<ShotEval> candidateShots(const World& w);

// P(legal pot of e.targetId) under Gaussian aim/speed execution noise.
double evaluate(const World& w, const ShotEval& e, int nRollouts,
                std::mt19937& rng, double aimSigmaRad = 0.009,
                double speedRelSigma = 0.05);

// Best single shot for the current legal target. Deterministic given seed.
ShotEval bestShot(const World& w, int nRollouts = 64, unsigned seed = 1);

}  // namespace cue
