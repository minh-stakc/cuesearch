#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "core/cloth.h"
#include "core/frame.h"
#include "engine/cuestrike.h"
#include "engine/motion.h"
#include "engine/resolve_ballball.h"

using namespace cue;
namespace {
const double R = k::R;
constexpr double DEG = 180.0 / 3.14159265358979323846;

double angDeg(const Vec3& a, const Vec3& b) {
    double an = std::sqrt(a.x * a.x + a.z * a.z);
    double bn = std::sqrt(b.x * b.x + b.z * b.z);
    double c = (a.x * b.x + a.z * b.z) / (an * bn);
    c = c < -1 ? -1 : (c > 1 ? 1 : c);
    return std::acos(c) * DEG;
}
double angTo_x(const Vec3& v) {  // |angle| to +x in xz-plane
    return std::fabs(std::atan2(v.z, v.x)) * DEG;
}
}  // namespace

TEST_CASE("squirt: zero at centre, monotone, banded, opposite english") {
    Ball c0;
    cueStrike(c0, {1, 0, 0}, 2.5, 0.0, 0.0);              // centre
    REQUIRE(angTo_x(c0.v) < 1e-9);

    Ball lo, hi;
    cueStrike(lo, {1, 0, 0}, 2.5, 0.20 * R, 0.0);
    cueStrike(hi, {1, 0, 0}, 2.5, 0.45 * R, 0.0);
    const double aLo = angDeg(lo.v, Vec3{1, 0, 0});
    const double aHi = angDeg(hi.v, Vec3{1, 0, 0});
    INFO("squirt lo=" << aLo << " hi=" << aHi);
    REQUIRE(aHi > aLo);                                    // monotone
    REQUIRE(aLo > 0.2);
    REQUIRE(aHi < 3.0);                                    // Dr. Dave band
    REQUIRE(hi.v.z < 0.0);            // right english (+z) -> squirt to -z
}

TEST_CASE("post-collision follow arc; draw is the mirror") {
    auto postCollisionTurn = [](double spinScale) {
        // Cue cuts a resting ball at 25 deg with follow/draw spin.
        Ball A, B;
        const double V = 2.5, phi = 25.0 / DEG;
        Vec3 d{std::cos(phi), 0.0, std::sin(phi)};
        B.r = {1.0, R, 0.6}; B.v = {}; B.w = {};
        A.r = B.r - d * (2.0 * R);
        A.v = {V, 0, 0};
        A.w = {0.0, 0.0, spinScale * V / R};   // <0 follow, >0 draw (aim +x)
        resolveBallBall(A, B);

        Segment s = beginSegment(A);
        REQUIRE(s.state == BallState::Sliding);            // slip after impact
        const Vec3 d0 = s.start.v;                          // departure dir
        Ball roll = s.endBall();
        REQUIRE(roll.state == BallState::Rolling);
        const Vec3 d1 = roll.v;                             // post-arc dir
        REQUIRE(angDeg(d0, d1) > 1.0);                      // a real arc
        return angTo_x(d1) - angTo_x(d0);   // <0: curved toward aim (+x)
    };
    const double follow = postCollisionTurn(-1.3);          // topspin
    const double draw   = postCollisionTurn(+1.3);          // backspin
    REQUIRE(follow < 0.0);             // follow curls the cue toward aim
    REQUIRE(draw   > 0.0);             // draw curls it the other way
}

TEST_CASE("cloth speed: slide window and curve scale as 1/muSlide") {
    Ball b;
    b.r = {0.5, R, 0.6};
    b.v = {2.0, 0.0, 0.0};
    b.w = {0.0, 0.0, 0.0};                                  // pure slide

    Segment f = beginSegment(b, fastCloth());               // muSlide 0.15
    Segment s = beginSegment(b, slowCloth());               // muSlide 0.30
    REQUIRE(f.state == BallState::Sliding);

    const double ratio = slowCloth().muSlide / fastCloth().muSlide;  // 2.0
    REQUIRE(f.T / s.T == Catch::Approx(ratio).margin(0.05));

    auto sideOffset = [](const Segment& seg) {
        Vec3 e = seg.at(seg.T).r;
        Vec3 straight = seg.start.r + seg.start.v * seg.T;
        return (e - straight).norm();
    };
    REQUIRE(sideOffset(f) / sideOffset(s) ==
            Catch::Approx(ratio).margin(0.10));             // curve ~ 1/mu
}

// NOTE: Dr. Dave's exact trisect rule (total cue deflection = 3 x cut, for
// good-action draw, cut < ~40 deg) needs his specific "good-action"
// calibration and measurement convention. Rather than tune constants until
// a number matches, we validate the unambiguous relational physics the rule
// is built on -- the cue's deflection off the object-ball line is strictly
// monotone in vertical spin: follow < stun (~90 deg) < draw. The precise 3x
// coefficient is a DEFERRED validation (documented limitation, not faked).
TEST_CASE("cue deflection is strictly monotone in spin (follow<stun<draw)") {
    auto deflectionFromOBLine = [](double spinScale) {
        Ball A, B;
        const double V = 2.5, phi = 20.0 / DEG;
        Vec3 d{std::cos(phi), 0.0, std::sin(phi)};       // line of centres
        B.r = {1.0, R, 0.6}; B.v = {}; B.w = {};
        A.r = B.r - d * (2.0 * R);
        A.v = {V, 0, 0};
        A.w = {0.0, 0.0, spinScale * V / R};   // <0 follow, 0 stun, >0 draw
        resolveBallBall(A, B);
        Segment s = beginSegment(A);
        Vec3 d1 = s.endBall().v;                          // settled direction
        return angDeg(d, d1);                             // angle off OB line
    };
    const double follow = deflectionFromOBLine(-1.0);
    const double stun   = deflectionFromOBLine(0.0);
    const double draw   = deflectionFromOBLine(+1.0);
    INFO("deflection follow=" << follow << " stun=" << stun
                              << " draw=" << draw);
    REQUIRE(stun == Catch::Approx(90.0).margin(8.0));     // ~tangent line
    REQUIRE(follow < stun);                                // follow pulls in
    REQUIRE(draw   > stun);                                // draw pushes out
}

TEST_CASE("cloth refactor preserves default-cloth behaviour") {
    // beginSegment default ClothParams == k:: constants -> CP1 numbers hold.
    Ball b;
    b.r = {0, R, 0};
    b.v = {2.0, 0, 0};
    Segment s = beginSegment(b);
    REQUIRE(s.T ==
            Catch::Approx(2.0 * 2.0 / (7.0 * k::MU_SLIDE * k::G)).margin(1e-12));
}
