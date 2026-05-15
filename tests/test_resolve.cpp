#include "engine/resolve_ballball.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "core/constants.h"
#include "core/frame.h"

using namespace cue;
namespace {
const double R = k::R, M = k::M;
constexpr double DEG = 180.0 / 3.14159265358979323846;

double planarAngleDeg(const Vec3& a, const Vec3& b) {
    double an = std::sqrt(a.x * a.x + a.z * a.z);
    double bn = std::sqrt(b.x * b.x + b.z * b.z);
    double c = (a.x * b.x + a.z * b.z) / (an * bn);
    c = c < -1 ? -1 : (c > 1 ? 1 : c);
    return std::acos(c) * DEG;
}

// Cue ball A at speed V along +x, hitting resting B with line-of-centres at
// cut angle phiDeg. Returns A,B post-resolution; d = line-of-centres unit.
void shoot(double V, double phiDeg, Vec3 spin, double muO, Ball& A, Ball& B,
           Vec3& d, double eO = -1.0) {
    const double phi = phiDeg / DEG;
    d = Vec3{std::cos(phi), 0.0, std::sin(phi)};
    B.r = {1.0, R, 0.6};
    B.v = {};
    B.w = {};
    A.r = B.r - d * (2.0 * R);
    A.v = {V, 0.0, 0.0};
    A.w = spin;
    resolveBallBall(A, B, muO, eO);
}

}  // namespace

TEST_CASE("textbook 90-degree rule (e=1) and exact e=0.95 deviation") {
    for (double phi : {15.0, 30.0, 45.0, 60.0}) {
        // Idealised: frictionless, perfectly elastic -> exactly 90 degrees.
        Ball A, B; Vec3 d;
        shoot(2.0, phi, Vec3{}, 0.0, A, B, d, 1.0);             // mu=0, e=1
        REQUIRE(planarAngleDeg(A.v, B.v) ==
                Catch::Approx(90.0).margin(0.5));
        REQUIRE(planarAngleDeg(B.v, d) < 0.5);                  // no throw

        // Physical e=0.95: equal-mass restitution predicts the cue retains
        // (1-e)/2 of the normal speed -> separation = atan(tanphi*2/(1-e)).
        Ball A2, B2; Vec3 d2;
        shoot(2.0, phi, Vec3{}, 0.0, A2, B2, d2);               // default e
        const double e = k::E_BALL;
        const double predicted =
            std::atan(std::tan(phi / DEG) * 2.0 / (1.0 - e)) * DEG;
        REQUIRE(planarAngleDeg(A2.v, B2.v) ==
                Catch::Approx(predicted).margin(0.5));
    }
}

TEST_CASE("linear momentum conserved; energy non-increasing") {
    Ball A, B; Vec3 d;
    Vec3 p0;
    {
        Ball a, b; a.v = {2.0, 0, 0}; b.v = {};
        p0 = (a.v + b.v) * M;
    }
    shoot(2.0, 35.0, Vec3{3.0, 1.0, -2.0}, -1.0, A, B, d);
    Vec3 p1 = (A.v + B.v) * M;
    REQUIRE(p1.x == Catch::Approx(p0.x).margin(1e-9));
    REQUIRE(p1.z == Catch::Approx(p0.z).margin(1e-9));

    double ke0 = 0.5 * M * 4.0;                                 // A only, 2 m/s
    double ke1 = kineticEnergy(A.v, A.w) + kineticEnergy(B.v, B.w);
    REQUIRE(ke1 <= ke0 + 1e-9);
}

TEST_CASE("cut-induced throw: peaks ~half-ball, slow speed (Dr. Dave)") {
    auto throwAt = [](double phi) {
        Ball A, B; Vec3 d;
        shoot(1.0, phi, Vec3{}, -1.0, A, B, d);                 // no spin
        return planarAngleDeg(B.v, d);
    };
    const double t10 = throwAt(10.0);
    const double t30 = throwAt(30.0);
    const double t60 = throwAt(60.0);

    INFO("throw  10=" << t10 << "  30=" << t30 << "  60=" << t60);
    REQUIRE(t30 > t10);                       // rises toward half-ball
    REQUIRE(t30 > t60);                       // falls past half-ball
    REQUIRE(t30 > 3.0);                        // physically significant
    REQUIRE(t30 < 8.0);                        // Dr. Dave ~5.8, honest band
    REQUIRE(t10 < 3.5);
}

TEST_CASE("no tangential slip -> zero throw (full-ball & gearing)") {
    Ball A, B; Vec3 d;
    // Full-ball: velocity along line of centres, no spin -> no slip.
    shoot(2.0, 0.0, Vec3{}, -1.0, A, B, d);
    REQUIRE(planarAngleDeg(B.v, d) < 0.2);

    // Gearing: at a cut, vertical sidespin chosen so the line-of-centres
    // contact slip cancels. In that frame tangential vA = -V*sin(phi) and
    // R*w_local_z = R*w.y, so gearing spin is w = (0, -V*sin(phi)/R, 0).
    Ball A2, B2; Vec3 d2;
    const double V = 2.0, phi = 30.0;
    shoot(V, phi, Vec3{0.0, -V * std::sin(phi / DEG) / R, 0.0}, -1.0,
          A2, B2, d2);
    REQUIRE(planarAngleDeg(B2.v, d2) < 0.5);
}

TEST_CASE("resolution is deterministic") {
    Ball A1, B1, A2, B2; Vec3 d;
    shoot(2.4, 28.0, Vec3{5, -2, 3}, -1.0, A1, B1, d);
    shoot(2.4, 28.0, Vec3{5, -2, 3}, -1.0, A2, B2, d);
    REQUIRE(A1.v.x == A2.v.x);
    REQUIRE(A1.w.z == A2.w.z);
    REQUIRE(B1.v.x == B2.v.x);
    REQUIRE(B1.v.z == B2.v.z);
}
