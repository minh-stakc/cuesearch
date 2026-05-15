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
    const double Rb = 2.0 * k::R;

    // Rail lines (the geometric mirror for bank/kick aim heuristics; the
    // real cushion physics in the rollout corrects the approximation, and
    // EV ranks the lower-fidelity bank/kick below a clean direct shot).
    struct Rail { int id; bool isX; double v; };
    const Rail rails[4] = {{0, true,  w.table.xMin}, {1, true,  w.table.xMax},
                           {2, false, w.table.zMin}, {3, false, w.table.zMax}};
    auto mirror = [](const Vec3& q, const Rail& r) {
        return r.isX ? Vec3{2.0 * r.v - q.x, q.y, q.z}
                     : Vec3{q.x, q.y, 2.0 * r.v - q.z};
    };
    auto crossesRail = [](const Vec3& a, const Vec3& b, const Rail& r) {
        const double ca = r.isX ? a.x : a.z, cb = r.isX ? b.x : b.z;
        return (ca - r.v) * (cb - r.v) < 0.0;          // opposite sides
    };

    const double spD[] = {1.6, 2.6, 3.4};
    const double bD[]  = {-0.30 * k::R, 0.0, 0.30 * k::R};
    const double spBK[] = {2.2, 3.2};                   // leaner grid

    auto push = [&](const Vec3& aim, int pk, ShotKind kind, int rail,
                    const double* sp, int ns, const double* bo, int nb) {
        for (int i = 0; i < ns; ++i)
            for (int j = 0; j < nb; ++j) {
                ShotEval e;
                e.shot = {aim, sp[i], 0.0, bo[j]};
                e.targetId = tgtId;
                e.pocket = pk;
                e.kind = kind;
                e.rail = rail;
                out.push_back(e);
            }
    };

    for (int pk = 0; pk < 6; ++pk) {
        const Vec3 P = pockets[pk];

        // --- Direct ghost-ball shot -------------------------------------
        const Vec3 dTP = planar(P - tgt).normalized();
        const Vec3 ghost = tgt - dTP * Rb;
        const Vec3 aim = planar(ghost - cuePos).normalized();
        const bool dirFeasible =
            aim.dot(dTP) >= std::cos(75.0 * 3.14159265 / 180.0) &&
            planar(P - tgt).dot(planar(tgt - cuePos)) > 0.0;
        if (dirFeasible)
            push(aim, pk, ShotKind::Direct, -1, spD, 3, bD, 3);

        // --- Bank: send the target toward the mirrored pocket -----------
        for (const Rail& r : rails) {
            const Vec3 VP = mirror(P, r);              // virtual pocket
            if (!crossesRail(tgt, VP, r)) continue;     // bank point exists
            const Vec3 dTV = planar(VP - tgt).normalized();
            const Vec3 g = tgt - dTV * Rb;
            const Vec3 a = planar(g - cuePos).normalized();
            if (a.dot(dTV) < std::cos(70.0 * 3.14159265 / 180.0)) continue;
            push(a, pk, ShotKind::Bank, r.id, spBK, 2, bD + 1, 1);
        }

        // --- Kick: cue off a rail to reach the ghost (snooker escape) ---
        for (const Rail& r : rails) {
            const Vec3 VG = mirror(ghost, r);           // virtual ghost
            if (!crossesRail(cuePos, VG, r)) continue;
            const Vec3 a = planar(VG - cuePos).normalized();
            push(a, pk, ShotKind::Kick, r.id, spBK, 2, bD + 1, 1);
        }
    }
    return out;
}

std::vector<ShotEval> safetyCandidates(const World& w) {
    std::vector<ShotEval> out;
    const int ci = cueIdx(w.balls);
    const int tgtId = legalTarget(w.balls);
    if (ci < 0 || tgtId < 0) return out;
    const Vec3 cuePos = w.balls[ci].r;
    const Vec3 tgt = w.balls[idxOfId(w.balls, tgtId)].r;

    auto add = [&](const Vec3& aim, int rail) {
        ShotEval e;
        e.shot = {aim, 1.0, 0.0, 0.0};               // soft -> a legal hit
        e.targetId = tgtId;
        e.pocket = -1;
        e.kind = ShotKind::Safety;
        e.rail = rail;
        out.push_back(e);
    };

    // (1) Direct soft contact.
    add(planar(tgt - cuePos).normalized(), -1);

    // (2) The SINGLE best one-rail kick-safety -- required to legally
    // contact a SNOOKERED ball (can't reach a blocked ball without a rail).
    // Pick the crossing rail with the shortest cue->mirrored-target path.
    // Minimal by design (advisor): one class, no two-rail / lag safeties.
    struct Rail { int id; bool isX; double v; };
    const Rail rails[4] = {{0, true,  w.table.xMin}, {1, true,  w.table.xMax},
                           {2, false, w.table.zMin}, {3, false, w.table.zMax}};
    int bestRail = -1;
    double bestLen = 1e30;
    Vec3 bestAim;
    for (const Rail& r : rails) {
        const Vec3 VT = r.isX ? Vec3{2.0 * r.v - tgt.x, tgt.y, tgt.z}
                              : Vec3{tgt.x, tgt.y, 2.0 * r.v - tgt.z};
        const double ca = r.isX ? cuePos.x : cuePos.z;
        const double cb = r.isX ? VT.x : VT.z;
        if ((ca - r.v) * (cb - r.v) >= 0.0) continue;  // no rail crossing
        const double len = planar(VT - cuePos).norm();
        if (len < bestLen) {
            bestLen = len;
            bestRail = r.id;
            bestAim = planar(VT - cuePos).normalized();
        }
    }
    if (bestRail >= 0) add(bestAim, bestRail);
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
