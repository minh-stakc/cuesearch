// Calibration of the single execution-noise sigma against a good player's
// skill curve (user anchor): P(pot) ~1.0 dead-straight, declining to ~0.60
// at the steepest practical cut. Uses CP7's VALIDATED geometry (cue+target
// in line to corner pocket 0; CP7 proved P=1.0 there) and induces the cut
// by sliding the cue laterally -- no bespoke geometry to get wrong.
#include "solver/solver.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "core/constants.h"

using namespace cue;
namespace {
const double R = k::R;

// Cue + target on the proven line to corner pocket 0, target ~0.28 m off
// the pocket, cue `gap` behind it, then shifted sideways by `lat` (m) to
// create a cut. lat=0 is the validated straight shot (CP7: pPot~1.0).
double potP(double lat, int N = 300, unsigned seed = 11) {
    World w;
    const Vec3 P = w.table.pockets()[0];
    const Vec3 out = Vec3{1, 0, 1}.normalized();        // away from pocket 0
    const Vec3 side{-out.z, 0.0, out.x};                // perpendicular
    Ball t; t.type = BallType::Object; t.id = 1;
    t.r = P + out * 0.28;
    Ball c; c.type = BallType::Cue; c.id = 0;
    c.r = t.r + out * 0.34 + side * lat;
    w.balls = {c, t};

    ShotEval best = bestShot(w, N, seed);               // uses k::AIM_SIGMA
    return best.targetId == 1 ? best.pPot : 0.0;
}
}  // namespace

TEST_CASE("pot curve vs cut: straight ~1.0 monotone down to a steep cut") {
    const double s0  = potP(0.00);   // straight (CP7-validated geometry)
    const double s1  = potP(0.10);   // small cut
    const double s2  = potP(0.20);   // moderate cut
    const double s3  = potP(0.30);   // steep cut
    INFO("pPot  lat0=" << s0 << "  0.10=" << s1 << "  0.20=" << s2
                        << "  0.30=" << s3 << "  (sigma=" << k::AIM_SIGMA
                        << ")");
    REQUIRE(s0 >= 0.90);                                // good player ~10/10
    REQUIRE(s0 >= s1 - 0.02);                           // harder => not higher
    REQUIRE(s1 >= s2 - 0.02);
    REQUIRE(s2 >= s3 - 0.02);
    REQUIRE(s3 <= s0);                                  // steep clearly lower
}
