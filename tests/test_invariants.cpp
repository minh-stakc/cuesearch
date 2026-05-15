// Conservation invariants as regression oracles: across many randomised
// shots through the full scheduler, total kinetic energy must be
// monotonically non-increasing, no ball may escape the table footprint, and
// every shot must terminate. These catch integrated-system bugs the
// per-component gates miss.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <vector>

#include "core/frame.h"
#include "engine/cuestrike.h"
#include "engine/world.h"

using namespace cue;
namespace { const double R = k::R; }

TEST_CASE("randomised shots: energy monotone, no escape, terminating") {
    std::mt19937 rng(2024);
    std::uniform_real_distribution<double> X(0.35, 2.15), Z(0.30, 0.97),
        SP(1.5, 4.0), OFF(-0.4 * R, 0.4 * R), ANG(-3.14159, 3.14159);

    int shots = 0;
    for (int it = 0; it < 150; ++it) {
        World w;
        std::vector<Ball> balls;
        auto freeSpot = [&](Vec3& p) {
            for (int tries = 0; tries < 50; ++tries) {
                p = {X(rng), R, Z(rng)};
                bool ok = true;
                for (auto& b : balls)
                    if ((b.r - p).norm() < 2.5 * R) ok = false;
                for (auto& pk : w.table.pockets())
                    if ((pk - p).norm() < w.table.pocketR + R) ok = false;
                if (ok) return true;
            }
            return false;
        };
        Vec3 p;
        if (!freeSpot(p)) continue;
        Ball cue; cue.r = p; cue.type = BallType::Cue; cue.id = 0;
        for (int j = 1; j <= 3; ++j) {
            if (!freeSpot(p)) break;
            Ball o; o.r = p; o.type = BallType::Object; o.id = j;
            balls.push_back(o);
        }
        const double a = std::cos(ANG(rng));
        cueStrike(cue, Vec3{a, 0.0, std::sqrt(1.0 - a * a)}, SP(rng),
                  OFF(rng), OFF(rng));
        balls.insert(balls.begin(), cue);
        w.balls = balls;

        double prevE = 1e300;
        bool escaped = false;
        const double t = w.simulate(
            [&](double, const WorldEvent&, const std::vector<Ball>& bs) {
                double E = 0.0;
                for (const Ball& b : bs) {
                    E += kineticEnergy(b.v, b.w);
                    if (b.pocketed) continue;
                    if (b.r.x < w.table.xMin - 1e-6 ||
                        b.r.x > w.table.xMax + 1e-6 ||
                        b.r.z < w.table.zMin - 1e-6 ||
                        b.r.z > w.table.zMax + 1e-6)
                        escaped = true;
                }
                REQUIRE(E <= prevE + 1e-7 * (1.0 + std::fabs(prevE)));
                prevE = E;
            },
            4096, 120.0);

        REQUIRE_FALSE(escaped);
        REQUIRE(t < 120.0);                        // terminated, not capped
        bool anyMoving = false;
        for (const Ball& b : w.balls)
            if (b.v.norm() > 1e-6) anyMoving = true;
        REQUIRE_FALSE(anyMoving);                   // came fully to rest
        ++shots;
    }
    REQUIRE(shots > 120);                           // the sweep actually ran
}
