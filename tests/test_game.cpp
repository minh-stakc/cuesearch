#include "engine/game.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "core/frame.h"
#include "engine/cuestrike.h"

using namespace cue;
namespace {
const double R = k::R;
bool has(const std::vector<int>& v, int x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}
}  // namespace

TEST_CASE("legalTarget is the lowest non-pocketed object ball") {
    std::vector<Ball> b(3);
    b[0].type = BallType::Cue;   b[0].id = 0;
    b[1].type = BallType::Object; b[1].id = 3;
    b[2].type = BallType::Object; b[2].id = 7;
    REQUIRE(legalTarget(b) == 3);
    b[1].pocketed = true;
    REQUIRE(legalTarget(b) == 7);
}

TEST_CASE("legal pot: cue strikes lowest ball, it falls, no foul") {
    World w;
    const Vec3 P = w.table.pockets()[0];          // corner (xMin,zMin)
    Vec3 dir = (Vec3{1, 0, 1}).normalized();      // outward from that pocket
    Ball one; one.type = BallType::Object; one.id = 1;
    one.r = P + dir * 0.20;
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = P + dir * (0.20 + 4.0 * R);
    cueStrike(cue, dir * -1.0, 2.4, 0.0, 0.0);    // stun, straight at it
    w.balls = {cue, one};

    ShotOutcome o = simulateShot(w);
    REQUIRE(o.firstContact == 1);
    REQUIRE(has(o.pocketed, 1));
    REQUIRE(o.foul == Foul::None);
    REQUIRE_FALSE(o.won);
}

TEST_CASE("scratch: cue driven straight into a pocket") {
    World w;
    const Vec3 P = w.table.pockets()[0];
    Vec3 dir = (Vec3{1, 0, 1}).normalized();
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = P + dir * 0.30;
    Ball one; one.type = BallType::Object; one.id = 1;
    one.r = {2.2, R, 1.0};                        // out of the way
    cueStrike(cue, dir * -1.0, 2.2, 0.0, 0.0);
    w.balls = {cue, one};

    ShotOutcome o = simulateShot(w);
    REQUIRE(o.cueScratched);
    REQUIRE(o.foul == Foul::Scratch);
}

TEST_CASE("no contact and wrong-ball-first fouls") {
    {   // No contact: cue rolls along open mid-table and stops short of any
        // rail or pocket (x=1.27 is the SIDE-pocket line -- avoid it).
        World w;
        Ball cue; cue.type = BallType::Cue; cue.id = 0;
        cue.r = {1.0, R, 0.5};
        Ball one; one.type = BallType::Object; one.id = 1;
        one.r = {0.30, R, 1.0};                          // off the path
        cueStrike(cue, Vec3{1, 0, 0}, 0.5, 0.0, 0.0);    // dies mid-table
        w.balls = {cue, one};
        ShotOutcome o = simulateShot(w);
        REQUIRE(o.firstContact == -1);
        REQUIRE_FALSE(o.cueScratched);
        REQUIRE(o.foul == Foul::NoContact);
    }
    {   // Wrong ball first: target is 1, cue hits 2.
        World w;
        Ball cue; cue.type = BallType::Cue; cue.id = 0;
        cue.r = {1.2, R, 0.63};
        Ball one; one.type = BallType::Object; one.id = 1;
        one.r = {0.4, R, 0.2};                          // not in the line
        Ball two; two.type = BallType::Object; two.id = 2;
        two.r = {1.2 + 6 * R, R, 0.63};                 // dead ahead
        cueStrike(cue, Vec3{1, 0, 0}, 2.0, 0.0, 0.0);
        w.balls = {cue, one, two};
        ShotOutcome o = simulateShot(w);
        REQUIRE(o.firstContact == 2);
        REQUIRE(o.foul == Foul::WrongBallFirst);
    }
}

TEST_CASE("win: potting the 9 on a legal shot") {
    World w;
    const Vec3 P = w.table.pockets()[0];
    Vec3 dir = (Vec3{1, 0, 1}).normalized();
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = P + dir * 0.20;
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = P + dir * (0.20 + 4.0 * R);
    cueStrike(cue, dir * -1.0, 2.4, 0.0, 0.0);
    w.balls = {cue, nine};

    ShotOutcome o = simulateShot(w);
    REQUIRE(o.firstContact == 9);
    REQUIRE(has(o.pocketed, 9));
    REQUIRE(o.foul == Foul::None);
    REQUIRE(o.won);
}
