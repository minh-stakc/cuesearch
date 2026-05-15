#include "solver/winsolve.h"

#include <catch2/catch_test_macros.hpp>

#include "core/constants.h"

using namespace cue;
namespace { const double R = k::R; }

TEST_CASE("planWin is deterministic for a fixed seed") {
    World w;
    const Vec3 P = w.table.pockets()[0];
    const Vec3 o = Vec3{1, 0, 1}.normalized();
    Ball t; t.type = BallType::Object; t.id = 1; t.r = P + o * 0.28;
    Ball c; c.type = BallType::Cue;   c.id = 0; c.r = t.r + o * 0.30;
    w.balls = {c, t};

    WinPlan a = planWin(w, 0, 6, 2, 5);
    WinPlan b = planWin(w, 0, 6, 2, 5);
    REQUIRE(a.winProb == b.winProb);
    REQUIRE(a.shot.kind == b.shot.kind);
    REQUIRE(a.shot.shot.speed == b.shot.shot.speed);
}

TEST_CASE("clean gimme on the 9: win-EV shoots to WIN, does not play safe") {
    World w;
    const Vec3 P = w.table.pockets()[0];
    const Vec3 o = Vec3{1, 0, 1}.normalized();
    // The 9 as a hanger -> potting it legally wins the rack outright.
    Ball t; t.type = BallType::Object; t.id = 9; t.r = P + o * 0.12;
    Ball c; c.type = BallType::Cue;   c.id = 0; c.r = t.r + o * 0.30;
    w.balls = {c, t};

    WinPlan p = planWin(w, 0, 8, 3, 9);
    REQUIRE(p.shot.kind != ShotKind::Safety);          // shoot the gimme
    REQUIRE(p.winProb > 0.5);
}

TEST_CASE("3-foul pressure: being on more fouls cannot raise win prob") {
    World w;
    Ball c; c.type = BallType::Cue;   c.id = 0; c.r = {1.0, R, 0.63};
    Ball one; one.type = BallType::Object; one.id = 1; one.r = {2.0, R, 0.63};
    Ball blk; blk.type = BallType::Object; blk.id = 5; blk.r = {1.5, R, 0.63};
    w.balls = {c, one, blk};                            // snooker

    const double v0 = planWin(w, 0, 6, 2, 3).winProb;
    const double v2 = planWin(w, 2, 6, 2, 3).winProb;   // already on 2 fouls
    REQUIRE(v2 <= v0 + 1e-9);                            // monotone in fouls
    REQUIRE(v0 >= 0.0);
    REQUIRE(v0 <= 1.0);
}
