#include "solver/solver.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>

#include "core/constants.h"
#include "engine/cuestrike.h"

using namespace cue;
namespace {
const double R = k::R;

// Cue + one target on the line to corner pocket 0, separated by `gap`.
World layout(double gap) {
    World w;
    const Vec3 P = w.table.pockets()[0];           // (xMin, R, zMin)
    const Vec3 out = Vec3{1, 0, 1}.normalized();
    Ball t; t.type = BallType::Object; t.id = 1;
    t.r = P + out * 0.25;
    Ball c; c.type = BallType::Cue; c.id = 0;
    c.r = t.r + out * gap;
    w.balls = {c, t};
    return w;
}
}  // namespace

TEST_CASE("candidates respect the miscue clip") {
    World w = layout(0.30);
    for (const ShotEval& e : candidateShots(w))
        REQUIRE(e.shot.a * e.shot.a + e.shot.b * e.shot.b <=
                (0.5 * R) * (0.5 * R) + 1e-12);
    REQUIRE_FALSE(candidateShots(w).empty());
}

TEST_CASE("solver finds a high-probability makeable shot") {
    World w = layout(0.30);                          // straight, in line
    ShotEval best = bestShot(w, 48, 7);
    REQUIRE(best.targetId == 1);
    REQUIRE(best.pPot > 0.6);

    // Execute the chosen shot with NO noise -> it must legally pot the 1.
    World x = w;
    cueStrike(x.balls[0], best.shot.aim, best.shot.speed, best.shot.a,
              best.shot.b);
    ShotOutcome o = simulateShot(x);
    REQUIRE(std::find(o.pocketed.begin(), o.pocketed.end(), 1) !=
            o.pocketed.end());
    REQUIRE(o.foul == Foul::None);
}

TEST_CASE("P(pot) calibrates: easy shot beats a thin long one") {
    World easy = layout(0.30);

    World hard;
    Ball t; t.type = BallType::Object; t.id = 1;
    t.r = {1.27, R, 0.63};                            // mid-table
    Ball c; c.type = BallType::Cue; c.id = 0;
    c.r = {1.05, R, 1.05};                            // thin cut, long
    hard.balls = {c, t};

    const double pEasy = bestShot(easy, 48, 3).pPot;
    const double pHard = bestShot(hard, 48, 3).pPot;
    INFO("pEasy=" << pEasy << " pHard=" << pHard);
    REQUIRE(pEasy > 0.6);
    REQUIRE(pEasy > pHard);
}

TEST_CASE("solver is deterministic for a fixed seed") {
    World w = layout(0.30);
    ShotEval a = bestShot(w, 32, 99);
    ShotEval b = bestShot(w, 32, 99);
    REQUIRE(a.pPot == b.pPot);
    REQUIRE(a.pocket == b.pocket);
    REQUIRE(a.shot.speed == b.shot.speed);
    REQUIRE(a.shot.aim.x == b.shot.aim.x);
}
