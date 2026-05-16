#pragma once
// RO-2: canonical mobility value over the difficulty table + an
// inverse-physics leave generator. The literature's value function
// (PickPocket/CueCard: 1*p1 + 0.33*p2 + 0.15*p3) adapted to 9-ball's
// ORDERED constraint: p1..p3 = best pot prob of the next THREE legal
// balls (lowest first), each from the difficulty table. The leave
// generator SEEDS (speed, vertical-spin) from the tangent/30/trisect
// model + d=v^2/(2 mu_r g) -- so we compute the leave instead of blindly
// searching (POS-a's failure mode), then a few simulate-and-correct
// steps. Additive: planRunout/CP7-8/shape untouched.
#include "solver/solver.h"

namespace cue {

// Mobility value of a state for the side to move (0..~1.48). Fast: table
// lookups + line-of-sight, no live MC. difficulty() must be built first.
double mobilityValue(const World& w);

// A shot that pots `targetId` via `pocketIdx` and leaves the cue near
// `leave`. Seeded from the inverse cue-ball-control model, then refined
// by a few noiseless simulate-and-correct steps. potsTarget=false if no
// makeable shot was found.
struct LeaveShot {
    ShotEval shot;
    bool potsTarget = false;
    double leaveErr = 1e9;   // achieved |cue_rest - leave| (m)
};
LeaveShot seedLeaveShot(const World& w, int targetId, int pocketIdx,
                        const Vec3& leave);

// RO-3: two-level run-out search (CueCard structure). Goal-directed
// candidates (pot the legal ball into each feasible pocket, leaving the
// cue at zones that set up the NEXT ball -- via seedLeaveShot), scored by
// table P(pot) * mobilityValue (fast, no live MC); top-k expanded one
// more level; defensive flag when nothing makeable. Deterministic.
struct RunOutPlan {
    ShotEval shot;
    double value = 0.0;     // chained P(pot)*mobility, depth-limited
    bool defensive = false; // no makeable shot -> caller should play safe
};
RunOutPlan planRunOut(const World& w, int depth = 2, int beamK = 3);

}  // namespace cue
