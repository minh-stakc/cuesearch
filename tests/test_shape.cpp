#include "solver/shape.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "core/constants.h"
#include "engine/cuestrike.h"

using namespace cue;
namespace {
const double R = k::R;

// State value after executing a chosen shot with NO noise.
double leaveAfter(World w, const ShotEval& s) {
    int ci = -1;
    for (size_t i = 0; i < w.balls.size(); ++i)
        if (w.balls[i].type == BallType::Cue) ci = (int)i;
    cueStrike(w.balls[ci], s.shot.aim, s.shot.speed, s.shot.a, s.shot.b);
    simulateShot(w);
    return posShapeValue(w);
}

// 1 makeable to the bottom-left corner; 2 across the table; 3 parked out
// of the way. THREE balls => no single shot can clear the rack, so the
// next-ball value reflects the LEAVE, not an accidental clearance.
World posScenario() {
    World w;
    const Vec3 P0 = w.table.pockets()[0];               // bottom-left
    Ball b1;  b1.type = BallType::Object; b1.id = 1;
    b1.r = P0 + Vec3{1, 0, 1}.normalized() * 0.26;
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = b1.r + Vec3{1, 0, 1}.normalized() * 0.34;   // straight on the 1
    Ball b2;  b2.type = BallType::Object; b2.id = 2;
    b2.r = {1.95, R, 0.95};
    Ball b3;  b3.type = BallType::Object; b3.id = 3;
    b3.r = {2.30, R, 0.30};                              // out of the way
    w.balls = {cue, b1, b2, b3};
    return w;
}
}  // namespace

TEST_CASE("posShapeValue: cleared rack ideal; harder shot lower") {
    World cleared;
    Ball c; c.type = BallType::Cue; c.id = 0; c.r = {1, R, 0.6};
    cleared.balls = {c};
    REQUIRE(posShapeValue(cleared) == Catch::Approx(1.0));

    World easy;
    const Vec3 P = easy.table.pockets()[0];
    Ball ce; ce.type = BallType::Cue; ce.id = 0;
    Ball o;  o.type = BallType::Object; o.id = 1;
    o.r = P + Vec3{1, 0, 1}.normalized() * 0.20;
    ce.r = o.r + Vec3{1, 0, 1}.normalized() * 0.30;
    easy.balls = {ce, o};

    World hard = easy;
    hard.balls[0].r = {2.4, R, 1.15};                 // long, thin, far
    REQUIRE(posShapeValue(easy) > posShapeValue(hard));
}

TEST_CASE("shapeShot pots the legal ball and is deterministic") {
    World w = posScenario();
    ShapeResult a = shapeShot(w);
    ShapeResult b = shapeShot(w);
    REQUIRE(a.potsTarget);
    REQUIRE(a.shot.targetId == 1);
    REQUIRE(a.shot.shot.speed == b.shot.shot.speed);
    REQUIRE(a.shot.shot.b == b.shot.shot.b);
    REQUIRE(a.leaveValue == b.leaveValue);
}

TEST_CASE("shape-aware leave is no worse than naive max-pot (and pots)") {
    World w = posScenario();

    ShotEval naive = bestShot(w, 48, 5);               // CP7 pot-EV, no shape
    REQUIRE(naive.targetId == 1);
    const double naiveLeave = leaveAfter(w, naive);

    ShapeResult shaped = shapeShot(w);
    REQUIRE(shaped.potsTarget);
    const double shapedLeave = leaveAfter(w, shaped.shot);

    INFO("naive leave=" << naiveLeave << "  shaped leave=" << shapedLeave);
    // The shaper explicitly optimises the leave: it must never set up the
    // next ball worse than a shape-blind max-pot shot.
    REQUIRE(shapedLeave >= naiveLeave - 1e-6);
    REQUIRE(shapedLeave > 0.0);                        // a real next shot
}
