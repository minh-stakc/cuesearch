#include "solver/plan.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>

#include "core/constants.h"
#include "engine/cuestrike.h"

using namespace cue;
namespace {
const double R = k::R;

// Two-ball 9-ball rack: 1 then 9, both makeable.
World rack2() {
    World w;
    const Vec3 P0 = w.table.pockets()[0];          // (xMin,zMin)
    const Vec3 P3 = w.table.pockets()[3];          // (xMax,zMax)
    Ball one;  one.type = BallType::Object; one.id = 1;
    one.r = P0 + Vec3{1, 0, 1}.normalized() * 0.28;
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = P3 + Vec3{-1, 0, -1}.normalized() * 0.45;
    Ball c;   c.type = BallType::Cue; c.id = 0;
    c.r = one.r + Vec3{1, 0, 1}.normalized() * 0.30;
    w.balls = {c, one, nine};
    return w;
}

// Easy rack: both balls near-hangers at separate pockets -> a clean
// runout exists, so greedy no-noise execution must complete it.
World rack2easy() {
    World w;
    const Vec3 P0 = w.table.pockets()[0];          // (xMin,zMin)
    const Vec3 P2 = w.table.pockets()[2];          // (xMin,zMax)
    Ball one;  one.type = BallType::Object; one.id = 1;
    one.r = P0 + Vec3{1, 0, 1}.normalized() * 0.10;
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = P2 + Vec3{1, 0, -1}.normalized() * 0.10;
    Ball c;   c.type = BallType::Cue; c.id = 0;
    c.r = {1.0, R, 0.63};
    w.balls = {c, one, nine};
    return w;
}

double nextBallProb(World x) {                     // P(pot the new target)
    return bestShot(x, 32, 5).pPot;
}
}  // namespace

TEST_CASE("plan is deterministic for a fixed seed") {
    World w = rack2();
    PlanResult a = planRunout(w, 2, 16, 3, 2, 42);
    PlanResult b = planRunout(w, 2, 16, 3, 2, 42);
    REQUIRE(a.value == b.value);
    REQUIRE(a.shot.pocket == b.shot.pocket);
    REQUIRE(a.shot.shot.speed == b.shot.shot.speed);
}

TEST_CASE("value decomposition: value <= chosen pPot; depth1 == pPot") {
    World w = rack2();
    PlanResult d1 = planRunout(w, 1, 24, 4, 2, 7);
    REQUIRE(d1.value >= 0.0);
    REQUIRE(d1.value <= 1.0);
    REQUIRE(d1.value == Catch::Approx(d1.shot.pPot).margin(1e-12));

    PlanResult d2 = planRunout(w, 2, 24, 4, 2, 7);
    // value = P(pot) * E[continuation in 0..1]  =>  value <= P(pot).
    REQUIRE(d2.value <= d2.shot.pPot + 1e-9);
    REQUIRE(d2.value >= 0.0);
}

TEST_CASE("lookahead is never worse at setting up the next ball") {
    World w = rack2();
    PlanResult d1 = planRunout(w, 1, 24, 4, 2, 11);   // myopic
    PlanResult d2 = planRunout(w, 2, 24, 4, 2, 11);   // positional

    auto leaveAfter = [&](const ShotEval& s) {
        World x = w;
        cueStrike(x.balls[0], s.shot.aim, s.shot.speed, s.shot.a, s.shot.b);
        simulateShot(x);                              // pot ball 1
        return nextBallProb(x);                       // P(make ball 9 next)
    };
    const double cont1 = leaveAfter(d1.shot);
    const double cont2 = leaveAfter(d2.shot);
    INFO("continuation  myopic=" << cont1 << "  lookahead=" << cont2);
    REQUIRE(cont2 >= cont1 - 0.05);                   // lookahead >= myopia
}

TEST_CASE("greedy execution of the plan runs out the 2-ball rack") {
    World w = rack2easy();
    bool won = false;
    for (int ball = 0; ball < 2 && !won; ++ball) {
        PlanResult p = planRunout(w, 2, 24, 4, 2, 3);
        REQUIRE(p.shot.targetId >= 1);
        cueStrike(w.balls[0], p.shot.shot.aim, p.shot.shot.speed,
                  p.shot.shot.a, p.shot.shot.b);
        ShotOutcome o = simulateShot(w);
        REQUIRE(o.foul == Foul::None);
        if (o.won) won = true;
    }
    REQUIRE(won);                                     // 1 then 9, potted out
}
