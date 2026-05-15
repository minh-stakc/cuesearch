#pragma once
// 9-ball game-state layer the solver needs: legal target = lowest-numbered
// object ball on the table; core foul detection. 8-ball is a documented
// extension (single-legal-target keeps the solver's objective clean).
#include <vector>

#include "engine/world.h"

namespace cue {

enum class Foul { None, Scratch, WrongBallFirst, NoContact };

struct ShotOutcome {
    std::vector<int> pocketed;   // object-ball ids pocketed this shot
    int firstContact = -1;       // first object ball the cue touched (id)
    Foul foul = Foul::None;
    bool cueScratched = false;
    bool won = false;            // 9-ball: the 9 potted on a legal shot
};

// Lowest-id non-pocketed object ball (the only legal first contact in
// 9-ball). Returns -1 if none remain.
int legalTarget(const std::vector<Ball>& balls);

// Simulate the shot already loaded into w (cue ball's v,w set). Classifies
// pockets / first contact / foul / win against 9-ball rules.
ShotOutcome simulateShot(World& w);

}  // namespace cue
