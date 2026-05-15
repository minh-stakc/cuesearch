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

TEST_CASE("kick + bank candidates are generated; direct still preferred") {
    World w = layout(0.30);                          // clean direct shot
    auto cands = candidateShots(w);
    bool hasDirect = false, hasBank = false, hasKick = false;
    for (const ShotEval& e : cands) {
        hasDirect |= e.kind == ShotKind::Direct;
        hasBank   |= e.kind == ShotKind::Bank;
        hasKick   |= e.kind == ShotKind::Kick;
        if (e.kind != ShotKind::Direct) REQUIRE(e.rail >= 0);
    }
    REQUIRE(hasDirect);
    REQUIRE(hasBank);
    REQUIRE(hasKick);
    // EV must still pick a Direct shot when a clean one exists (no
    // regression of the emergent "prefer direct" behaviour).
    ShotEval best = bestShot(w, 48, 7);
    REQUIRE(best.kind == ShotKind::Direct);
    REQUIRE(best.pPot > 0.6);
}

TEST_CASE("snooker: blocked direct line still yields a legal kick option") {
    World w;
    Ball cue;  cue.type = BallType::Cue;   cue.id = 0; cue.r = {1.0, R, 0.63};
    Ball one;  one.type = BallType::Object; one.id = 1; one.r = {2.0, R, 0.63};
    Ball blk;  blk.type = BallType::Object; blk.id = 5;
    blk.r = {1.5, R, 0.63};                          // dead on the cue->1 line
    w.balls = {cue, one, blk};

    auto cands = candidateShots(w);
    bool kick = false;
    for (const ShotEval& e : cands)
        if (e.kind == ShotKind::Kick && e.targetId == 1) kick = true;
    REQUIRE(kick);                                    // an escape exists

    // The straight direct shot at the 1 is blocked by the 5: a no-noise
    // direct attempt must foul (wrong ball first), proving the kick is
    // not redundant.
    World x = w;
    cueStrike(x.balls[0], Vec3{1, 0, 0}, 2.5, 0.0, 0.0);
    ShotOutcome o = simulateShot(x);
    REQUIRE(o.firstContact == 5);
    REQUIRE(o.foul == Foul::WrongBallFirst);
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
