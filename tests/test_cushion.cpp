#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "core/frame.h"
#include "engine/resolve_cushion.h"
#include "engine/world.h"

using namespace cue;
namespace { const double R = k::R; }

TEST_CASE("pocket capture removes a ball headed for the mouth") {
    World w;
    Ball cue;
    cue.r = {0.45, R, 0.45};
    Vec3 dir = (Vec3{w.table.xMin, R, w.table.zMin} - cue.r).normalized();
    cue.v = dir * 3.0;
    cue.w = rollingSpin(cue.v, 0.0);
    w.balls = {cue};
    w.simulate();
    REQUIRE(w.balls[0].pocketed);
}

TEST_CASE("ball that stops short is not pocketed") {
    World w;
    Ball b;
    b.r = {1.2, R, 0.6};
    b.v = {0.05, 0, 0};                       // barely moves, dies mid-table
    b.w = rollingSpin(b.v, 0.0);
    w.balls = {b};
    w.simulate();
    REQUIRE_FALSE(w.balls[0].pocketed);
}

TEST_CASE("cushion: perpendicular no-spin loses ~half the speed") {
    Ball b;
    b.v = {0.0, 0.0, -3.0};                   // straight at the zMin rail
    b.w = {};
    const Vec3 o{0, 0, -1};                    // zMin outward normal
    resolveCushion(b, o);
    REQUIRE(b.v.z > 0.0);                                  // rebounded
    const double ret = std::fabs(b.v.z) / 3.0;
    INFO("cushion speed retention = " << ret);
    REQUIRE(ret > 0.4);
    REQUIRE(ret < 0.75);                                   // Dr. Dave ~0.5
}

TEST_CASE("cushion: running english lengthens, reverse shortens") {
    auto alongRailExit = [](double spinY) {
        Ball b;
        b.v = {0.0, 0.0, -3.0};               // into zMin rail
        b.w = {0.0, spinY, 0.0};              // sidespin about vertical
        resolveCushion(b, Vec3{0, 0, -1});
        return b.v.x;                          // rail-tangent component
    };
    const double reverse = alongRailExit(-40.0);
    const double none    = alongRailExit(0.0);
    const double running = alongRailExit(+40.0);
    INFO("alongRail rev=" << reverse << " none=" << none
                          << " run=" << running);
    REQUIRE(running > none);                    // running english -> longer
    REQUIRE(none > reverse);                    // reverse english -> shorter
}

TEST_CASE("world remains deterministic with cushions + pockets") {
    auto run = [] {
        World w;
        Ball c;
        c.r = {0.6, R, 0.4};
        c.v = {2.2, 0.0, 0.7};
        c.w = rollingSpin(c.v, 1.0);
        w.balls = {c};
        std::vector<Ball> fin;
        double t = w.simulate(
            [&](double, const WorldEvent&, const std::vector<Ball>& bs) {
                fin = bs;
            });
        return std::make_pair(t, fin);
    };
    auto [t1, f1] = run();
    auto [t2, f2] = run();
    REQUIRE(t1 == t2);
    REQUIRE(f1[0].r.x == f2[0].r.x);
    REQUIRE(f1[0].pocketed == f2[0].pocketed);
}
