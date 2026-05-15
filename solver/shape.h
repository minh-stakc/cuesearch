#pragma once
// Positional shape planning (additive; planRunout/CP7-8 untouched).
// "Optimise the LEAVE, not just the pot": for each feasible (ball,pocket)
// the cue contact is fixed by geometry; the free choice is where the cue
// ENDS UP. shapeShot solves (speed, vertical spin) so the cue leaves with a
// good angle on the NEXT legal ball.
#include "solver/solver.h"

namespace cue {

// Cheap-but-honest value of a state for the side to move: makeability of
// the next legal ball, factoring cut angle, distance, AND occlusion by the
// remaining balls. 0 = no shot, ~1 = easy. (docs/POSITIONAL_DESIGN.md)
double posShapeValue(const World& w);

struct ShapeResult {
    ShotEval shot;        // shot that pots the legal ball...
    double leaveValue;    // ...and the posShapeValue of the resulting state
    bool potsTarget;      // did the (noiseless) shot legally pot the target
};

// Best shape-aware shot for the current legal ball. Deterministic.
ShapeResult shapeShot(const World& w, unsigned seed = 1);

}  // namespace cue
