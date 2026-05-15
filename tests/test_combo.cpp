#include "solver/solver.h"

#include <catch2/catch_test_macros.hpp>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"

using namespace cue;
namespace { const double R = k::R; }

// Legal target 1, the 9 hanging at corner pocket 0, all collinear so a
// straight cue->1->9->pocket combo is an instant, legal win.
static World comboRack() {
    World w;
    const Vec3 d = Vec3{1, 0, 1}.normalized();         // out of pocket 0
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = d * 0.06;  nine.r.y = R;
    Ball one;  one.type = BallType::Object; one.id = 1;
    one.r = d * (0.06 + 4.0 * R);  one.r.y = R;
    Ball cue;  cue.type = BallType::Cue;   cue.id = 0;
    cue.r = one.r + d * (2.0 * R + 0.10);
    w.balls = {cue, one, nine};
    return w;
}

TEST_CASE("combo candidate is generated when the 9 hugs a pocket") {
    World w = comboRack();
    bool combo = false;
    for (const ShotEval& e : candidateShots(w))
        if (e.kind == ShotKind::Combo && e.targetId == 1) combo = true;
    REQUIRE(combo);
}

TEST_CASE("combo is NOT generated when the 9 is far from any pocket") {
    World w = comboRack();
    for (Ball& b : w.balls)
        if (b.id == 9) b.r = {1.27, R, 0.63};          // mid-table
    for (const ShotEval& e : candidateShots(w))
        REQUIRE(e.kind != ShotKind::Combo);
}

TEST_CASE("the combo geometry is a real, legal instant win") {
    World w = comboRack();
    const Vec3 d = Vec3{1, 0, 1}.normalized();
    cueStrike(w.balls[0], d * -1.0, 2.6, 0.0, 0.0);    // straight through
    ShotOutcome o = simulateShot(w);
    REQUIRE(o.firstContact == 1);                       // lowest hit first
    REQUIRE(o.foul == Foul::None);
    REQUIRE(o.won);                                     // the 9 fell -> win
}

TEST_CASE("candidate generation is deterministic") {
    World w = comboRack();
    auto a = candidateShots(w);
    auto b = candidateShots(w);
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i].kind == b[i].kind);
        REQUIRE(a[i].shot.aim.x == b[i].shot.aim.x);
    }
}
