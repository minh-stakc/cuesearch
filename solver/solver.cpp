#include "solver/solver.h"

#include <algorithm>
#include <cmath>
#include <future>
#include <thread>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"

namespace cue {
namespace {

int cueIdx(const std::vector<Ball>& b) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].type == BallType::Cue) return static_cast<int>(i);
    return -1;
}
int idxOfId(const std::vector<Ball>& b, int id) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].id == id) return static_cast<int>(i);
    return -1;
}
Vec3 planar(Vec3 v) { v.y = 0.0; return v; }

// Rotate a planar unit vector about +y by th.
Vec3 rotY(const Vec3& v, double th) {
    const double c = std::cos(th), s = std::sin(th);
    return {v.x * c + v.z * s, 0.0, -v.x * s + v.z * c};
}

}  // namespace

std::vector<ShotEval> candidateShots(const World& w) {
    std::vector<ShotEval> out;
    const int ci = cueIdx(w.balls);
    const int tgtId = legalTarget(w.balls);
    if (ci < 0 || tgtId < 0) return out;

    const Vec3 cuePos = w.balls[ci].r;
    const Vec3 tgt = w.balls[idxOfId(w.balls, tgtId)].r;
    const auto pockets = w.table.pockets();

    const double speeds[] = {1.4, 2.0, 2.8, 3.6};
    const double bOff[] = {-0.30 * k::R, 0.0, 0.30 * k::R};   // draw/stun/foll

    for (int pk = 0; pk < 6; ++pk) {
        const Vec3 P = pockets[pk];
        const Vec3 dTP = planar(P - tgt).normalized();         // target->pocket
        const Vec3 ghost = tgt - dTP * (2.0 * k::R);           // ghost ball
        const Vec3 aim = planar(ghost - cuePos).normalized();

        // Feasible only if the cue can drive the target toward the pocket
        // (cut angle < ~75 deg) and the pocket faces the target.
        const double cosCut = aim.dot(dTP);
        if (cosCut < std::cos(75.0 * 3.14159265 / 180.0)) continue;
        if (planar(P - tgt).dot(planar(tgt - cuePos)) <= 0.0) continue;

        for (double sp : speeds)
            for (double b : bOff) {
                ShotEval e;
                e.shot = {aim, sp, 0.0, b};                     // no english v1
                e.targetId = tgtId;
                e.pocket = pk;
                out.push_back(e);
            }
    }
    return out;
}

double evaluate(const World& w, const ShotEval& e, int nRollouts,
                unsigned baseSeed, double aimSigmaRad,
                double speedRelSigma) {
    // One rollout, seeded ONLY by (baseSeed, r): result is independent of
    // how the range is split -> deterministic under any thread count.
    const auto oneRollout = [&](int r) -> int {
        std::mt19937 rng(baseSeed * 2654435761u + static_cast<unsigned>(r));
        std::normal_distribution<double> nAim(0.0, aimSigmaRad);
        std::normal_distribution<double> nSpd(0.0, speedRelSigma);
        World ww = w;
        const int ci = cueIdx(ww.balls);
        cueStrike(ww.balls[ci], rotY(e.shot.aim, nAim(rng)),
                  e.shot.speed * (1.0 + nSpd(rng)), e.shot.a, e.shot.b);
        ShotOutcome o = simulateShot(ww);
        for (int id : o.pocketed)
            if (id == e.targetId && o.foul == Foul::None) return 1;
        return 0;
    };

    unsigned T = std::max(1u, std::thread::hardware_concurrency());
    T = std::min<unsigned>(T, static_cast<unsigned>(std::max(1, nRollouts)));
    std::vector<std::future<int>> tasks;
    for (unsigned t = 0; t < T; ++t) {
        const int lo = static_cast<int>(t) * nRollouts / static_cast<int>(T);
        const int hi =
            static_cast<int>(t + 1) * nRollouts / static_cast<int>(T);
        tasks.push_back(std::async(std::launch::async, [&, lo, hi] {
            int m = 0;
            for (int r = lo; r < hi; ++r) m += oneRollout(r);
            return m;
        }));
    }
    int made = 0;
    for (auto& f : tasks) made += f.get();
    return static_cast<double>(made) / nRollouts;
}

ShotEval bestShot(const World& w, int nRollouts, unsigned seed) {
    ShotEval best;
    int i = 0;
    for (ShotEval e : candidateShots(w)) {
        // Distinct, fixed per-candidate base seed -> order-independent.
        e.pPot = evaluate(w, e, nRollouts,
                          seed ^ (0x9E3779B1u * static_cast<unsigned>(++i)));
        if (e.pPot > best.pPot) best = e;
    }
    return best;
}

}  // namespace cue
