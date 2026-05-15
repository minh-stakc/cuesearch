#include "engine/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

#include "core/frame.h"
#include "engine/cuestrike.h"

using namespace cue;
namespace { const double R = k::R; }

TEST_CASE("positionPoly matches segment evaluation") {
    Ball b;
    b.r = {0.3, R, 0.5};
    b.v = {1.4, 0.0, -0.7};                  // pure slide (w=0)
    Segment s = beginSegment(b);
    REQUIRE(s.state == BallState::Sliding);

    Vec3 p0, p1, p2;
    s.positionPoly(p0, p1, p2);
    for (double tau : {0.0, 0.01, 0.05, s.T * 0.5, s.T}) {
        Vec3 fromPoly = p0 + p1 * tau + p2 * (tau * tau);
        Vec3 fromSeg = s.at(tau).r;
        REQUIRE(fromPoly.x == Catch::Approx(fromSeg.x).margin(1e-12));
        REQUIRE(fromPoly.z == Catch::Approx(fromSeg.z).margin(1e-12));
    }
}

TEST_CASE("ball-ball collision detected exactly at contact distance") {
    World w;
    Ball a;
    a.r = {0.4, R, 0.635};
    a.v = {2.0, 0.0, 0.0};
    a.w = rollingSpin(a.v, 0.0);             // rolling, no spin
    Ball t;
    t.r = {1.2, R, 0.635};                   // dead in line, stationary
    w.balls = {a, t};

    bool sawCollision = false;
    w.simulate([&](double, const WorldEvent& e,
                   const std::vector<Ball>& bs) {
        if (e.type == EventType::BallBall && !sawCollision) {
            sawCollision = true;
            const Vec3 d = bs[0].r - bs[1].r;
            REQUIRE(d.norm() == Catch::Approx(2.0 * R).margin(1e-7));
            // Post-CP3 physics: head-on equal-mass, e=0.95 -> target gets
            // (1+e)/2, cue follows through (1-e)/2 (~2.5%), not a dead stop.
            REQUIRE(bs[1].v.x > 0.1);                 // target set in motion
            REQUIRE(bs[0].v.x > 0.0);                 // cue follows through
            REQUIRE(bs[0].v.x < 0.1 * bs[1].v.x);     // ...but only slightly
        }
    });
    REQUIRE(sawCollision);
}

TEST_CASE("cushion keeps the ball on the table and reflects it") {
    World w;
    Ball a;
    a.r = {2.0, R, 0.635};
    a.v = {3.0, 0.0, 0.0};                    // straight at the +x rail
    a.w = rollingSpin(a.v, 0.0);
    w.balls = {a};

    bool reflected = false;
    w.simulate([&](double, const WorldEvent& e,
                   const std::vector<Ball>& bs) {
        if (e.type == EventType::Cushion) reflected = true;
        REQUIRE(bs[0].r.x <= w.table.cxMax() + 1e-6);
        REQUIRE(bs[0].r.x >= w.table.cxMin() - 1e-6);
        REQUIRE(bs[0].r.z <= w.table.czMax() + 1e-6);
        REQUIRE(bs[0].r.z >= w.table.czMin() - 1e-6);
    });
    REQUIRE(reflected);
}

TEST_CASE("cue strike: centre hit -> no spin; top hit -> follow") {
    Ball c;
    cueStrike(c, Vec3{1, 0, 0}, 2.5, 0.0, 0.0);   // dead centre
    REQUIRE(c.v.x > 0.0);
    REQUIRE(c.w.norm() < 1e-9);

    Ball top;
    cueStrike(top, Vec3{1, 0, 0}, 2.5, 0.0, 0.6 * R);  // above centre
    // Aim +x: pure rolling/follow spin is w = (0,0,-v/R) -> w.z < 0.
    REQUIRE(top.w.z < 0.0);
}

TEST_CASE("scheduler is bitwise-deterministic") {
    auto run = [](std::vector<Ball>& outFinal, int& count) {
        World w;
        Ball c;
        cueStrike(c, Vec3{1, 0, 0.15}, 3.0, 0.4 * R, 0.3 * R);
        c.r = {0.4, R, 0.635};
        Ball o;
        o.r = {1.5, R, 0.7};
        w.balls = {c, o};
        count = 0;
        double t = w.simulate(
            [&](double, const WorldEvent&, const std::vector<Ball>&) {
                ++count;
            });
        outFinal = w.balls;
        return t;
    };
    std::vector<Ball> f1, f2;
    int c1 = 0, c2 = 0;
    double t1 = run(f1, c1), t2 = run(f2, c2);

    REQUIRE(t1 == t2);
    REQUIRE(c1 == c2);
    REQUIRE(f1.size() == f2.size());
    for (size_t i = 0; i < f1.size(); ++i) {
        REQUIRE(f1[i].r.x == f2[i].r.x);
        REQUIRE(f1[i].r.z == f2[i].r.z);
        REQUIRE(f1[i].v.x == f2[i].v.x);
        REQUIRE(f1[i].w.y == f2[i].w.y);
    }
}
