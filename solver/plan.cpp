#include "solver/plan.h"

#include <algorithm>
#include <cmath>

#include "core/constants.h"
#include "engine/cuestrike.h"

namespace cue {
namespace {

int cueIdx(const std::vector<Ball>& b) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].type == BallType::Cue) return static_cast<int>(i);
    return -1;
}

// P(legal pot of e.targetId) AND up to `maxLeaves` resulting world states
// (post-shot, target pocketed, cue at rest) for the recursion.
double evalWithLeaves(const World& w, const ShotEval& e, int nRollouts,
                      std::mt19937& rng, int maxLeaves,
                      std::vector<World>& leaves) {
    std::normal_distribution<double> nAim(0.0, k::AIM_SIGMA);
    std::normal_distribution<double> nSpd(0.0, k::SPEED_SIGMA);
    int made = 0;
    for (int r = 0; r < nRollouts; ++r) {
        World ww = w;
        const int ci = cueIdx(ww.balls);
        const Vec3 aim = [&] {
            const double th = nAim(rng), c = std::cos(th), s = std::sin(th);
            return Vec3{e.shot.aim.x * c + e.shot.aim.z * s, 0.0,
                        -e.shot.aim.x * s + e.shot.aim.z * c};
        }();
        cueStrike(ww.balls[ci], aim, e.shot.speed * (1.0 + nSpd(rng)),
                  e.shot.a, e.shot.b);
        ShotOutcome o = simulateShot(ww);
        bool potted = std::find(o.pocketed.begin(), o.pocketed.end(),
                                e.targetId) != o.pocketed.end();
        if (potted && o.foul == Foul::None) {
            ++made;
            if (static_cast<int>(leaves.size()) < maxLeaves)
                leaves.push_back(ww);            // the resulting position
        }
    }
    return static_cast<double>(made) / nRollouts;
}

}  // namespace

PlanResult planRunout(const World& w, int depth, int nRollouts, int beamK,
                      int leaveSamples, unsigned seed) {
    std::mt19937 rng(seed);
    PlanResult best;
    best.depth = depth;

    std::vector<ShotEval> cands = candidateShots(w);
    if (cands.empty()) return best;

    // Cheap prefilter: rank by immediate P(pot), keep the beam.
    for (ShotEval& e : cands) {
        std::vector<World> dump;
        e.pPot = evalWithLeaves(w, e, std::max(8, nRollouts / 2), rng, 0,
                                dump);
    }
    std::sort(cands.begin(), cands.end(),
              [](const ShotEval& a, const ShotEval& b) {
                  return a.pPot > b.pPot;
              });
    if (static_cast<int>(cands.size()) > beamK) cands.resize(beamK);

    for (ShotEval& e : cands) {
        std::vector<World> leaves;
        const double pPot =
            evalWithLeaves(w, e, nRollouts, rng, leaveSamples, leaves);
        e.pPot = pPot;

        double value;
        if (depth <= 1 || leaves.empty() ||
            legalTarget(leaves.front().balls) < 0) {
            value = pPot;                         // terminal: just this pot
        } else {
            double acc = 0.0;
            for (const World& lv : leaves)
                acc += planRunout(lv, depth - 1, nRollouts, beamK,
                                  leaveSamples, seed + 1)
                           .value;
            value = pPot * (acc / leaves.size());  // P(pot)*E[continuation]
        }
        if (value > best.value) {
            best.shot = e;
            best.value = value;
        }
    }
    return best;
}

}  // namespace cue
