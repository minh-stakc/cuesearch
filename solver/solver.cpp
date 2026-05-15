#include "solver/solver.h"

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
                std::mt19937& rng, double aimSigmaRad,
                double speedRelSigma) {
    std::normal_distribution<double> nAim(0.0, aimSigmaRad);
    std::normal_distribution<double> nSpd(0.0, speedRelSigma);
    int made = 0;
    for (int r = 0; r < nRollouts; ++r) {
        World ww = w;                                          // fresh copy
        const int ci = cueIdx(ww.balls);
        const Vec3 aim = rotY(e.shot.aim, nAim(rng));
        const double sp = e.shot.speed * (1.0 + nSpd(rng));
        cueStrike(ww.balls[ci], aim, sp, e.shot.a, e.shot.b);
        ShotOutcome o = simulateShot(ww);
        bool potted = false;
        for (int id : o.pocketed)
            if (id == e.targetId) potted = true;
        if (potted && o.foul == Foul::None) ++made;
    }
    return static_cast<double>(made) / nRollouts;
}

ShotEval bestShot(const World& w, int nRollouts, unsigned seed) {
    std::mt19937 rng(seed);
    ShotEval best;
    for (ShotEval e : candidateShots(w)) {
        e.pPot = evaluate(w, e, nRollouts, rng);
        if (e.pPot > best.pPot) best = e;
    }
    return best;
}

}  // namespace cue
