// RO-2 gate: (a) mobility value is sane (cleared=1.0; open rack > a
// blocked one); (b) the inverse-physics leave generator pots the ball AND
// gets the cue genuinely near a reachable target leave (real positional
// control -- the thing POS-a's blind coordinate descent could not do);
// (c) deterministic.
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

double cueRestErr(World w, const ShotEval& s, const Vec3& leave) {
    int ci = -1;
    for (size_t i = 0; i < w.balls.size(); ++i)
        if (w.balls[i].type == BallType::Cue) ci = (int)i;
    cueStrike(w.balls[ci], s.shot.aim, s.shot.speed, s.shot.a, s.shot.b);
    simulateShot(w);
    Vec3 d = w.balls[ci].r - leave; d.y = 0;
    return d.norm();
}
}  // namespace

TEST_CASE("RO-2: mobility value is sane") {
    difficultyMut().buildOrLoad("difficulty_test.bin", 24);

    World cleared;
    Ball cc; cc.type = BallType::Cue; cc.id = 0; cc.r = {1, R, 0.6};
    cleared.balls = {cc};
    REQUIRE(mobilityValue(cleared) == Catch::Approx(1.0));

    // Open rack: 1 has a clear short shot. Blocked: a wall in front of it.
    World open;
    const Vec3 P0 = open.table.pockets()[0];
    Ball o1; o1.type = BallType::Object; o1.id = 1;
    o1.r = P0 + Vec3{1, 0, 1}.normalized() * 0.30;
    Ball oc; oc.type = BallType::Cue; oc.id = 0;
    oc.r = o1.r + Vec3{1, 0, 1}.normalized() * 0.30;
    Ball o9; o9.type = BallType::Object; o9.id = 9; o9.r = {2.0, R, 0.6};
    open.balls = {oc, o1, o9};

    World blocked = open;
    Ball blk; blk.type = BallType::Object; blk.id = 5;
    blk.r = (open.balls[0].r + o1.r) * 0.5;          // dead on the line
    blocked.balls.push_back(blk);

    INFO("open=" << mobilityValue(open)
                 << " blocked=" << mobilityValue(blocked));
    REQUIRE(mobilityValue(open) > mobilityValue(blocked));
    REQUIRE(mobilityValue(open) > 0.5);
}

TEST_CASE("RO-2: inverse leave generator pots + controls the cue") {
    // A real ~35-deg CUT (straight shots have no shape leverage for ANY
    // method). Cue approaches the ghost at an angle to the OB->pocket
    // line, so follow/draw + speed genuinely redirect the cue.
    World w;
    const Vec3 P0 = w.table.pockets()[0];
    const Vec3 out = Vec3{1, 0, 1}.normalized();      // OB -> away-from-pocket
    Ball one; one.type = BallType::Object; one.id = 1;
    one.r = P0 + out * 0.34;
    Vec3 ghost = one.r + out * (2.0 * R);
    const double th = 35.0 * 3.14159265 / 180.0;
    Vec3 appr{out.x * std::cos(th) + out.z * std::sin(th), 0.0,
              -out.x * std::sin(th) + out.z * std::cos(th)};
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = ghost + appr * 0.45;
    w.balls = {cue, one};

    // A leave that requires FOLLOW: ahead of the natural stun (tangent)
    // landing, toward the cue's incoming line -- a blind stun misses it.
    Vec3 aim0 = (ghost - cue.r).normalized();
    Vec3 dOP = (P0 - one.r); dOP.y = 0; dOP = dOP.normalized();
    Vec3 tang = aim0 - dOP * aim0.dot(dOP);
    tang.y = 0; tang = tang.normalized();
    Vec3 leave = ghost + tang * 0.30 + aim0 * 0.35;   // tangent + follow
    leave.x = std::min(2.4, std::max(0.1, leave.x));
    leave.z = std::min(1.15, std::max(0.1, leave.z));

    LeaveShot ls = seedLeaveShot(w, 1, 0, leave);
    REQUIRE(ls.potsTarget);

    // The honest claim (like POS-a): the inverse-physics generator gives
    // MATERIALLY better cue control than a blind, position-unaware shot
    // (aim at the pocket, fixed medium stun). A leave is a zone, not a
    // point -- so also require it lands within a real shape zone (~0.4 m).
    Vec3 c0 = w.balls[0].r, ai = (ghost - c0).normalized();
    double naiveErr = cueRestErr(w, {{ai, 2.4, 0.0, 0.0}, 1, 0,
                                     ShotKind::Direct, -1, 0.0}, leave);
    INFO("seeded leaveErr=" << ls.leaveErr << "  blind=" << naiveErr);
    REQUIRE(ls.leaveErr < naiveErr - 0.10);           // clearly beats blind
    REQUIRE(ls.leaveErr < 0.40);                      // within a shape zone

    // Determinism.
    LeaveShot ls2 = seedLeaveShot(w, 1, 0, leave);
    REQUIRE(ls2.shot.shot.speed == ls.shot.shot.speed);
    REQUIRE(ls2.shot.shot.b == ls.shot.shot.b);
    REQUIRE(ls2.leaveErr == Catch::Approx(ls.leaveErr).margin(1e-9));
}
