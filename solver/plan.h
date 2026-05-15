#pragma once
// THE HEADLINE. Multi-shot positional lookahead: a shot's value is not
// P(pot) but P(legal pot) * E[ value of the resulting leave for the NEXT
// ball ], recursed to a depth budget with a beam over immediate P(pot).
// No open-source pool engine does this. It is beam search over a stochastic
// forward model -- the same shape as multi-step execution planning in quant.
#include "solver/solver.h"

namespace cue {

struct PlanResult {
    ShotEval shot;       // recommended first shot
    double value = 0.0;  // P(pot)*E[continuation], in [0,1]
    int depth = 0;
};

// depth = how many balls deep to plan (1 = myopic single-shot).
// Deterministic given seed.
PlanResult planRunout(const World& w, int depth, int nRollouts = 24,
                      int beamK = 4, int leaveSamples = 3, unsigned seed = 1);

}  // namespace cue
