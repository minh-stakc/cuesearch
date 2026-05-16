// RO-1 gate: the difficulty table must (a) build/cache/reload, (b) match
// the calibrated skill curve (straight ~1.0, monotone down in cut and
// distance -- the geometry-is-right sanity), (c) agree with a direct
// live MC at spot checks (the table is not a lie).
#include "solver/difficulty.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <random>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "engine/world.h"

using namespace cue;
namespace {
const double R = k::R;
constexpr double DEG = 3.14159265358979323846 / 180.0;

// Direct (non-table) reference MC for the same canonical geometry.
double liveP(double alphaDeg, double dCO, double dOP, int sims) {
    World base;
    const Vec3 P = base.table.pockets()[0];
    const Vec3 out = Vec3{1, 0, 1}.normalized();
    Vec3 O = P + out * dOP; O.y = R;
    Vec3 ghost = O + out * (2.0 * R);
    double th = alphaDeg * DEG;
    Vec3 ap{out.x * std::cos(th) + out.z * std::sin(th), 0.0,
            -out.x * std::sin(th) + out.z * std::cos(th)};
    Vec3 cue = O + ap * dCO; cue.y = R;
    Vec3 aim = (ghost - cue).normalized();
    std::mt19937 rng(7919u);
    std::normal_distribution<double> nA(0.0, k::AIM_SIGMA),
        nS(0.0, k::SPEED_SIGMA);
    int pot = 0;
    for (int s = 0; s < sims; ++s) {
        World w = base;
        Ball o; o.type = BallType::Object; o.id = 1; o.r = O;
        Ball c; c.type = BallType::Cue;   c.id = 0; c.r = cue;
        w.balls = {c, o};
        double t2 = nA(rng), cc = std::cos(t2), ssn = std::sin(t2);
        Vec3 a{aim.x * cc + aim.z * ssn, 0.0, -aim.x * ssn + aim.z * cc};
        cueStrike(w.balls[0], a, 3.0 * (1.0 + nS(rng)), 0.0, 0.0);
        ShotOutcome r = simulateShot(w);
        if (std::find(r.pocketed.begin(), r.pocketed.end(), 1) !=
            r.pocketed.end())
            ++pot;
    }
    return double(pot) / sims;
}
}  // namespace

TEST_CASE("RO-1: difficulty table builds, matches skill curve & live MC") {
    DifficultyTable t;
    t.buildOrLoad("difficulty_test.bin", 24);   // coarse: fast, cached
    REQUIRE(t.ready);

    // (a) calibrated-curve sanity (geometry is right): straight ~1.0,
    //     monotone non-increasing in cut and in distance.
    const double straight = t.potProb(0.0, 0.30, 0.30);
    INFO("straight=" << straight
         << "  a60=" << t.potProb(60.0, 0.30, 0.30)
         << "  far=" << t.potProb(0.0, 0.85, 0.85));
    REQUIRE(straight >= 0.90);
    REQUIRE(t.potProb(0.0, 0.30, 0.30) >= t.potProb(60.0, 0.30, 0.30));
    REQUIRE(t.potProb(0.0, 0.30, 0.30) >= t.potProb(0.0, 0.85, 0.85) - 0.02);

    // (b) the table is not a lie: spot-check vs a direct live MC.
    for (auto cfg : {std::array<double, 3>{15, 0.40, 0.40},
                     std::array<double, 3>{45, 0.50, 0.30}}) {
        double tv = t.potProb(cfg[0], cfg[1], cfg[2]);
        double lv = liveP(cfg[0], cfg[1], cfg[2], 200);
        INFO("cfg a=" << cfg[0] << " tbl=" << tv << " live=" << lv);
        REQUIRE(std::fabs(tv - lv) < 0.18);
    }

    // (c) cache round-trips.
    DifficultyTable t2;
    t2.buildOrLoad("difficulty_test.bin", 24);
    REQUIRE(t2.ready);
    REQUIRE(t2.potProb(20.0, 0.45, 0.45) ==
            Catch::Approx(t.potProb(20.0, 0.45, 0.45)).margin(1e-9));
}
