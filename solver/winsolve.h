#pragma once
// Win-probability objective: the project's thesis applied to the whole rack.
// We do NOT special-case "safety" -- the value function becomes P(my side
// wins the rack) under 2-ply self-play with the 9-ball 3-consecutive-foul
// terminal, and safeties EMERGE (a soft legal-contact shot that leaves the
// opponent nothing scores higher than a 12% kick because the math says so).
//
// Bounded by design (documented simplifications, not silent):
//  - opponent = same policy (self-play);
//  - 2 turn-pass plies hard cap, continuation reuses tested planRunout;
//  - ball-in-hand = flat +0.25 to the receiver's win prob, capped (NOT a
//    placement search);
//  - the 2-foul-warning rule is ignored;
//  - foul state is per-invocation (CLI may seed "fouls already on me").
#include "solver/solver.h"

namespace cue {

struct WinPlan {
    ShotEval shot;
    double winProb = 0.0;     // P(side-to-move wins the rack)
};

// foulsOnMover = consecutive fouls already against the side to move (0..2).
WinPlan planWin(const World& w, int foulsOnMover = 0, int nRollouts = 20,
                int beamK = 4, unsigned seed = 1);

}  // namespace cue
