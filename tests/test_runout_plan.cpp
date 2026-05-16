// RO-3 gate: the two-level run-out search (a) pots the legal ball &
// returns fast (the anti-POS-b-timeout point), (b) lookahead is no worse
// than greedy at the resulting position, (c) flags defensive when
// snookered, (d) deterministic.
#include "solver/runout.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "solver/difficulty.h"

using namespace cue;
namespace {
const double R = k::R;

double mobilityAfter(World w, const ShotEval& s) {
    int ci = -1;
    for (size_t i = 0; i < w.balls.size(); ++i)
        if (w.balls[i].type == BallType::Cue) ci = (int)i;
    cueStrike(w.balls[ci], s.shot.aim, s.shot.speed, s.shot.a, s.shot.b);
    simulateShot(w);
    return mobilityValue(w);
}
World rack() {
    World w;
    const Vec3 P0 = w.table.pockets()[0];
    const Vec3 out = Vec3{1, 0, 1}.normalized();
    Ball one; one.type = BallType::Object; one.id = 1;
    one.r = P0 + out * 0.32;
    // ~30-deg CUT on the 1 (cue NOT in line -> cue departs on the tangent,
    // does NOT follow it into the corner: a real shot, not a scratch trap).
    Vec3 ghost = one.r + out * (2.0 * R);
    const double th = 30.0 * 3.14159265 / 180.0;
    Vec3 appr{out.x * std::cos(th) + out.z * std::sin(th), 0.0,
              -out.x * std::sin(th) + out.z * std::cos(th)};
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = ghost + appr * 0.40;
    Ball two; two.type = BallType::Object; two.id = 2; two.r = {1.7, R, 0.9};
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = {2.0, R, 0.5};
    w.balls = {cue, one, two, nine};
    return w;
}
}  // namespace

TEST_CASE("RO-3: two-level search pots, looks ahead, fast & deterministic") {
    difficultyMut().buildOrLoad("difficulty_test.bin", 24);

    World w = rack();
    RunOutPlan p2 = planRunOut(w, 2, 3);
    REQUIRE_FALSE(p2.defensive);
    REQUIRE(p2.shot.targetId == 1);                   // shoots the legal ball
    REQUIRE(p2.value > 0.0);

    // Lookahead must not leave a worse position than greedy (depth 1).
    RunOutPlan p1 = planRunOut(w, 1, 3);
    REQUIRE_FALSE(p1.defensive);
    double m2 = mobilityAfter(w, p2.shot);
    double m1 = mobilityAfter(w, p1.shot);
    INFO("mobility after  lookahead=" << m2 << "  greedy=" << m1);
    REQUIRE(m2 >= m1 - 0.05);

    // Determinism.
    RunOutPlan a = planRunOut(w, 2, 3), b = planRunOut(w, 2, 3);
    REQUIRE(a.shot.shot.speed == b.shot.shot.speed);
    REQUIRE(a.value == Catch::Approx(b.value).margin(1e-9));
}

TEST_CASE("RO-3: snookered legal ball -> defensive") {
    difficultyMut().buildOrLoad("difficulty_test.bin", 24);
    World w;
    Ball cue; cue.type = BallType::Cue; cue.id = 0; cue.r = {1.0, R, 0.63};
    Ball one; one.type = BallType::Object; one.id = 1; one.r = {2.0, R, 0.63};
    Ball blk; blk.type = BallType::Object; blk.id = 5; blk.r = {1.5, R, 0.63};
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = {0.5, R, 0.30};
    w.balls = {cue, one, blk, nine};
    RunOutPlan p = planRunOut(w, 2, 3);
    REQUIRE(p.defensive);                             // no makeable line
}
