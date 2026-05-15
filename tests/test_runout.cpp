// POS-b gate (pre-committed in docs/POSITIONAL_DESIGN.md):
//   success: >= 60% full run-out (legally pot 1..9) from ball-in-hand on
//            the 1, clean spread rack, 100 noisy trials.
//   failure: < 25% after POS-a+b  ->  ship bounded, document, do NOT move
//            the goalposts.
#include "solver/shape.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>

#include "core/constants.h"
#include "engine/cuestrike.h"

using namespace cue;
namespace {
const double R = k::R;

// One representative OPEN 9-ball spread: every ball individually makeable,
// none clustered, none on a pocket. The rack is fixed; only execution
// noise varies across trials -> isolates positional skill from layout luck.
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

// Ball-in-hand on the 1: cue ~0.35 m behind it, straight to its best
// (lowest-cut, clear) pocket.
void ballInHandOn1(World& w) {
    const int ci = cueI(w);
    Vec3 O;
    for (const Ball& b : w.balls) if (b.id == 1) O = b.r;
    double best = -1; Vec3 bestPos = w.balls[ci].r;
    for (const Vec3& P : w.table.pockets()) {
        Vec3 d = P - O; d.y = 0;
        if (d.norm() < 1e-6) continue;
        d = d.normalized();
        Vec3 pos = O - d * 0.35; pos.y = R;
        if (pos.x < w.table.cxMin() || pos.x > w.table.cxMax() ||
            pos.z < w.table.czMin() || pos.z > w.table.czMax()) continue;
        World t = w; t.balls[ci].r = pos;
        double v = posShapeValue(t);
        if (v > best) { best = v; bestPos = pos; }
    }
    w.balls[ci].r = bestPos;
}
}  // namespace

TEST_CASE("POS-b gate: run-out rate from ball-in-hand on the 1") {
    const int TRIALS = 100;
    int runouts = 0, totalBalls = 0, reachedShot1 = 0;
    for (int trial = 0; trial < TRIALS; ++trial) {
        int ballsThisRun = 0;
        World w = cleanRack();
        ballInHandOn1(w);
        const int ci = cueI(w);
        std::mt19937 rng(1234u + trial);
        std::normal_distribution<double> nAim(0.0, 0.009), nSpd(0.0, 0.05);

        bool ok = true;
        for (int shot = 0; shot < 30 && ok; ++shot) {
            const int t = legalTarget(w.balls);
            if (t < 0) break;
            PlanShapeResult ps = planShape(w, 2, 7u * trial + shot);
            if (!ps.potsTarget || ps.shot.targetId < 0) { ok = false; break; }

            const double th = nAim(rng), c = std::cos(th), s = std::sin(th);
            Vec3 aim{ps.shot.shot.aim.x * c + ps.shot.shot.aim.z * s, 0.0,
                     -ps.shot.shot.aim.x * s + ps.shot.shot.aim.z * c};
            cueStrike(w.balls[ci], aim,
                      ps.shot.shot.speed * (1.0 + nSpd(rng)),
                      ps.shot.shot.a, ps.shot.shot.b);
            if (shot == 0) ++reachedShot1;
            ShotOutcome o = simulateShot(w);
            if (o.won && o.foul == Foul::None) {
                ++runouts; ++ballsThisRun; break;
            }
            if (o.foul != Foul::None) { ok = false; break; }
            bool potted = false;
            for (int id : o.pocketed) if (id == t) potted = true;
            if (!potted) { ok = false; break; }       // missed -> run ends
            ++ballsThisRun;
        }
        totalBalls += ballsThisRun;
    }
    // PRE-COMMITTED OUTCOME (docs/POSITIONAL_DESIGN.md): the >=60% success
    // bar was NOT met (measured run-out rate below, ~0%, far under the 25%
    // failure threshold). Per the pre-commitment we do NOT move the
    // goalposts and do NOT chase a fix: the shape planner is shipped
    // OPT-IN ONLY and never replaces the validated bounded solver on any
    // default path. This test asserts the experiment genuinely ran
    // end-to-end (all trials executed) and records the honest metric; the
    // go/no-go verdict and its consequence live in the design doc + NOTES.
    INFO("run-out rate = " << runouts << " / " << TRIALS
         << "   avg balls/run = " << (double)totalBalls / TRIALS
         << "   reached-shot-1 = " << reachedShot1 << "/" << TRIALS);
    REQUIRE(reachedShot1 == TRIALS);                  // harness ran in full
    REQUIRE(runouts * 100 < 60 * TRIALS);             // gate NOT met (honest)
}
