// MEASUREMENT (not a capability gate -- nothing was pre-committed here):
// how often does the SHIPPED default solver run out a clean rack from
// ball-in-hand on the 1, at the calibrated player level (k::AIM_SIGMA)?
// Uses solve's real routing: planRunout (pot-EV, depth 2); if it finds no
// makeable shot a safety would pass the turn -> the single-player run
// ends. This quantifies the documented bounded-solver ceiling; it is
// expected to be low (it can pot, it cannot string position).
#include "solver/plan.h"
#include "solver/solver.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

#include "core/constants.h"
#include "engine/cuestrike.h"

using namespace cue;
namespace {
const double R = k::R;

World cleanRack() {
    World w;
    auto add = [&](int id, double x, double z) {
        Ball b; b.type = BallType::Object; b.id = id; b.r = {x, R, z};
        w.balls.push_back(b);
    };
    Ball cue; cue.type = BallType::Cue; cue.id = 0; cue.r = {0.40, R, 0.63};
    w.balls.push_back(cue);
    add(1, 0.55, 0.40); add(2, 0.60, 0.95); add(3, 1.00, 0.58);
    add(4, 1.35, 0.30); add(5, 1.35, 1.00); add(6, 1.70, 0.62);
    add(7, 2.05, 0.38); add(8, 2.05, 0.95); add(9, 1.55, 0.63);
    return w;
}
int cueI(const World& w) {
    for (size_t i = 0; i < w.balls.size(); ++i)
        if (w.balls[i].type == BallType::Cue) return (int)i;
    return -1;
}
void ballInHandOn1(World& w) {                 // cue ~0.35 m behind the 1
    const int ci = cueI(w);
    Vec3 O;
    for (const Ball& b : w.balls) if (b.id == 1) O = b.r;
    double best = -1; Vec3 bp = w.balls[ci].r;
    for (const Vec3& P : w.table.pockets()) {
        Vec3 d = P - O; d.y = 0;
        if (d.norm() < 1e-6) continue;
        d = d.normalized();
        Vec3 pos = O - d * 0.35; pos.y = R;
        if (pos.x < w.table.cxMin() || pos.x > w.table.cxMax() ||
            pos.z < w.table.czMin() || pos.z > w.table.czMax()) continue;
        World t = w; t.balls[ci].r = pos;
        ShotEval s = bestShot(t, 16, 3);
        if (s.targetId == 1 && s.pPot > best) { best = s.pPot; bp = pos; }
    }
    w.balls[ci].r = bp;
}
}  // namespace

TEST_CASE("MEASURE: shipped-solver run-out rate, calibrated player") {
    const int TRIALS = 30;   // reduced sample: planRunout depth-2 is heavy
    int runouts = 0, totalBalls = 0, ran = 0;
    for (int trial = 0; trial < TRIALS; ++trial) {
        World w = cleanRack();
        ballInHandOn1(w);
        const int ci = cueI(w);
        std::mt19937 rng(900u + trial);
        std::normal_distribution<double> nA(0.0, k::AIM_SIGMA),
            nS(0.0, k::SPEED_SIGMA);
        int balls = 0;
        for (int shot = 0; shot < 30; ++shot) {
            const int t = legalTarget(w.balls);
            if (t < 0) break;
            if (shot == 0) ++ran;
            // Shipped routing: pot-EV positional planner; shoot if a
            // makeable line exists (else a safety would end the run).
            PlanResult pr = planRunout(w, 2, 14, 3, 2, 7u * trial + shot);
            if (pr.shot.targetId < 0 || pr.shot.pPot <= 0.05) break;

            const double th = nA(rng), c = std::cos(th), s = std::sin(th);
            Vec3 aim{pr.shot.shot.aim.x * c + pr.shot.shot.aim.z * s, 0.0,
                     -pr.shot.shot.aim.x * s + pr.shot.shot.aim.z * c};
            cueStrike(w.balls[ci], aim,
                      pr.shot.shot.speed * (1.0 + nS(rng)),
                      pr.shot.shot.a, pr.shot.shot.b);
            ShotOutcome o = simulateShot(w);
            if (o.won && o.foul == Foul::None) { ++runouts; ++balls; break; }
            if (o.foul != Foul::None) break;
            bool potted = false;
            for (int id : o.pocketed) if (id == t) potted = true;
            if (!potted) break;
            ++balls;
        }
        totalBalls += balls;
    }
    INFO("SHIPPED solver run-out = " << runouts << "/" << TRIALS
         << "   avg balls/run = " << (double)totalBalls / TRIALS
         << "   (sigma=" << k::AIM_SIGMA << ")");
    REQUIRE(ran == TRIALS);                 // measurement actually executed
}
