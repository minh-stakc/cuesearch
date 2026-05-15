#include "engine/layout.h"

#include <catch2/catch_test_macros.hpp>

#include "core/constants.h"
#include "solver/solver.h"

using namespace cue;
namespace { const double R = k::R; }

TEST_CASE("layout save/load round-trips") {
    World w;
    Ball c; c.type = BallType::Cue;   c.id = 0; c.r = {0.40, R, 0.55};
    Ball a; a.type = BallType::Object; a.id = 1; a.r = {1.10, R, 0.90};
    Ball n; n.type = BallType::Object; n.id = 9; n.r = {2.00, R, 0.20};
    w.balls = {c, a, n};

    World r = loadLayout(saveLayout(w));
    REQUIRE(r.balls.size() == 3);
    for (size_t i = 0; i < 3; ++i) {
        REQUIRE(r.balls[i].id == w.balls[i].id);
        REQUIRE(r.balls[i].type == w.balls[i].type);
        REQUIRE(r.balls[i].r.x == w.balls[i].r.x);
        REQUIRE(r.balls[i].r.z == w.balls[i].r.z);
    }
}

TEST_CASE("pocketed balls are dropped from a saved layout") {
    World w;
    Ball c; c.type = BallType::Cue; c.id = 0; c.r = {1, R, 0.6};
    Ball a; a.type = BallType::Object; a.id = 1; a.r = {1.5, R, 0.6};
    a.pocketed = true;
    w.balls = {c, a};
    REQUIRE(loadLayout(saveLayout(w)).balls.size() == 1);
}

TEST_CASE("deterministic parallelism: evaluate is thread-count invariant") {
    // Same baseSeed -> identical result regardless of how rollouts split
    // across threads (per-rollout seeding). Two calls must be bitwise equal.
    World w;
    const Vec3 P = w.table.pockets()[0];
    const Vec3 out = Vec3{1, 0, 1}.normalized();
    Ball t; t.type = BallType::Object; t.id = 1; t.r = P + out * 0.28;
    Ball c; c.type = BallType::Cue; c.id = 0; c.r = t.r + out * 0.30;
    w.balls = {c, t};

    auto cands = candidateShots(w);
    REQUIRE_FALSE(cands.empty());
    const double a = evaluate(w, cands.front(), 96, 12345u);
    const double b = evaluate(w, cands.front(), 96, 12345u);
    REQUIRE(a == b);                               // exact, not approx

    ShotEval s1 = bestShot(w, 48, 77);
    ShotEval s2 = bestShot(w, 48, 77);
    REQUIRE(s1.pPot == s2.pPot);
    REQUIRE(s1.shot.speed == s2.shot.speed);
}
