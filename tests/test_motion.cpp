#include "engine/motion.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

#include "core/frame.h"

using namespace cue;

namespace {
constexpr double V = 2.0;            // launch speed (m/s)
const double g = k::G, R = k::R;
const double muS = k::MU_SLIDE, muR = k::MU_ROLL, muSp = k::MU_SPIN;
}  // namespace

TEST_CASE("pure forward slide: T, 5/7 rule, roll-consistent spin") {
    Ball b;
    b.r = {0, R, 0};
    b.v = {V, 0, 0};
    b.w = {};

    Segment s = beginSegment(b);
    REQUIRE(s.state == BallState::Sliding);
    REQUIRE(s.T == Catch::Approx(2.0 * V / (7.0 * muS * g)).margin(1e-12));

    Ball end = s.endBall();
    // Classic result: a sliding ball reaches natural roll at 5/7 v0.
    REQUIRE(end.v.norm() == Catch::Approx(5.0 * V / 7.0).margin(1e-9));
    REQUIRE(end.state == BallState::Rolling);
    // Rolling no-slip invariant: slip == 0 at the transition.
    REQUIRE(slip(end.v, end.w).norm() < 1e-9);
    REQUIRE(end.w.z == Catch::Approx(-5.0 * V / (7.0 * R)).margin(1e-6));
}

TEST_CASE("natural-roll launch skips the slide phase") {
    Ball b;
    b.r = {0, R, 0};
    b.v = {V, 0, 0};
    b.w = rollingSpin(b.v, 0.0);          // already rolling
    Segment s = beginSegment(b);
    REQUIRE(s.state == BallState::Rolling);
    REQUIRE(s.T == Catch::Approx(V / (muR * g)).margin(1e-9));
}

TEST_CASE("pure vertical spin decays to stationary") {
    Ball b;
    b.r = {0, R, 0};
    b.w = {0, 30.0, 0};                   // spin in place
    Segment s = beginSegment(b);
    REQUIRE(s.state == BallState::Spinning);
    REQUIRE(s.T == Catch::Approx(30.0 / (2.5 * muSp * g / R)).margin(1e-9));
    REQUIRE(s.endBall().state == BallState::Stationary);
}

TEST_CASE("full sequence: total time and terminal rest") {
    Ball b;
    b.r = {0, R, 0};
    b.v = {V, 0, 0};

    double tEnd = simulateToRest(b, nullptr, 2e-3);
    const double t1 = 2.0 * V / (7.0 * muS * g);          // slide
    const double t2 = (5.0 * V / 7.0) / (muR * g);        // roll to stop
    REQUIRE(tEnd == Catch::Approx(t1 + t2).margin(1e-6));
}

TEST_CASE("energy is monotonically non-increasing") {
    Ball b;
    b.r = {0, R, 0};
    b.v = {V, 0.0, 0.6 * V};               // angled
    b.w = {5.0, 12.0, -3.0};               // side + vertical + follow spin

    std::vector<double> E;
    simulateToRest(b,
                   [&](double, const Ball& s) {
                       E.push_back(kineticEnergy(s.v, s.w));
                   },
                   2e-3);

    REQUIRE(E.size() > 100);
    const double tol = 1e-9 * E.front();
    for (size_t i = 1; i < E.size(); ++i)
        REQUIRE(E[i] <= E[i - 1] + tol);
}

TEST_CASE("deterministic: identical inputs -> bitwise-identical result") {
    Ball b;
    b.r = {0, R, 0};
    b.v = {1.7, 0.0, -0.9};
    b.w = {2.1, -4.0, 0.5};

    Ball f1, f2;
    double t1 = simulateToRest(
        b, [&](double, const Ball& s) { f1 = s; }, 2e-3);
    double t2 = simulateToRest(
        b, [&](double, const Ball& s) { f2 = s; }, 2e-3);

    REQUIRE(t1 == t2);                       // exact, not approx
    REQUIRE(f1.r.x == f2.r.x);
    REQUIRE(f1.r.z == f2.r.z);
    REQUIRE(f1.v.x == f2.v.x);
    REQUIRE(f1.w.y == f2.w.y);
    REQUIRE(f1.state == f2.state);
    REQUIRE(f1.state == BallState::Stationary);
}
