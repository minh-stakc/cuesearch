// CP-pre: does the engine reproduce known LEAVES (post-shot cue resting
// position) through the INTEGRATED path (world scheduler / simulateShot)?
// Positional planning is the first thing to use the engine recursively;
// if leaves are wrong here, a shape solver hallucinates against its own
// simulation. These gate POS-a.
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "core/frame.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "engine/world.h"

using namespace cue;
namespace { const double R = k::R; }

TEST_CASE("leave A: a rolling ball stops at v^2 / (2 mu_r g)") {
    World w;
    Ball c;
    c.type = BallType::Cue; c.id = 0;
    c.r = {0.30, R, 0.635};
    const double V = 0.60;                 // small enough to stop on cloth
    c.v = {V, 0.0, 0.0};
    c.w = rollingSpin(c.v, 0.0);           // pure natural roll
    w.balls = {c};
    w.simulate();

    const double d = V * V / (2.0 * k::MU_ROLL * k::G);   // analytic
    REQUIRE(w.balls[0].r.x == Catch::Approx(0.30 + d).margin(0.01));
    REQUIRE(w.balls[0].r.z == Catch::Approx(0.635).margin(1e-6));
}

TEST_CASE("leave B: cue leave responds to spin (draw < stun < follow)") {
    auto leaveX = [](double b) {
        World w;
        Ball cue; cue.type = BallType::Cue; cue.id = 0;
        cue.r = {0.50, R, 0.635};
        Ball obj; obj.type = BallType::Object; obj.id = 1;
        obj.r = {1.10, R, 0.635};           // straight, dead in line
        w.balls = {cue, obj};
        cueStrike(w.balls[0], Vec3{1, 0, 0}, 2.2, 0.0, b);
        simulateShot(w);
        return w.balls[0].r.x;              // cue resting x
    };
    const double draw = leaveX(-0.30 * R);
    const double stun = leaveX(0.0);
    const double foll = leaveX(+0.30 * R);
    INFO("leave x  draw=" << draw << " stun=" << stun << " follow=" << foll);
    REQUIRE(draw < stun);                   // backspin -> cue ends behind
    REQUIRE(stun < foll);                   // topspin -> cue ends ahead
}

TEST_CASE("leave C: short stun CUT -> cue LEAVES along the tangent line") {
    World w;
    Ball obj; obj.type = BallType::Object; obj.id = 1;
    obj.r = {1.10, R, 0.635};
    // Send the object +x: ghost is 2R behind it on that line. Approach the
    // ghost from an ANGLE (a real cut, not a full hit) and from CLOSE range
    // (still sliding = genuine stun at contact, no developed roll).
    Vec3 ghost = obj.r - Vec3{2.0 * R, 0, 0};
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = {ghost.x - 0.12, R, ghost.z + 0.06};
    w.balls = {cue, obj};

    Vec3 aim = ghost - cue.r; aim.y = 0; aim = aim.normalized();
    cueStrike(w.balls[0], aim, 2.0, 0.0, 0.0);   // pure stun, short cut

    // The "leave" is the cue's motion AFTER contact, not its total
    // displacement from the start (which includes the approach leg along
    // the line of centres). Capture the cue velocity at the BallBall event.
    Vec3 cueVafter{};
    bool sawHit = false;
    w.simulate([&](double, const WorldEvent& e, const std::vector<Ball>& bs) {
        if (e.type == EventType::BallBall && !sawHit) {
            sawHit = true;
            cueVafter = bs[0].v;                  // cue == index 0
        }
    });
    REQUIRE(sawHit);

    // Line of centres at contact is object - ghost = +x; tangent is its
    // perpendicular (the z-axis). A stun cut leaves the cue ~along it.
    Vec3 loc = (obj.r - ghost); loc.y = 0; loc = loc.normalized();
    Vec3 tangent{-loc.z, 0.0, loc.x};
    cueVafter.y = 0.0;
    REQUIRE(cueVafter.norm() > 0.05);             // cue retains tangent speed
    double align = std::fabs(cueVafter.normalized().dot(tangent));
    INFO("post-contact tangent alignment = " << align);
    REQUIRE(align > 0.92);                         // ~within ~23 deg (e<1)
}
