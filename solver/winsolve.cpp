#include "solver/winsolve.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "solver/plan.h"

namespace cue {
namespace {

int cueIdx(const std::vector<Ball>& b) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].type == BallType::Cue) return static_cast<int>(i);
    return -1;
}

constexpr double kBIH = 0.25;   // flat ball-in-hand bonus to the receiver

// P(side-to-move wins). ply = remaining turn-pass budget.
double moverWin(const World& w, int foulsOnMover, int ply, int nRollouts,
                int beamK, unsigned seed);

// Position value for the side to move: a BOUNDED proxy = how makeable the
// legal ball is by the single best direct shot (~36 sims). NOT the full
// multi-ball runout (that re-generates bank/kick candidates and explodes
// when called inside the recursion). Documented simplification: this values
// "did I leave them a shot?", not "can they run the whole rack" -- adequate
// for the shoot-vs-safe decision, and tractable. Called per STATE only.
double posValue(const World& w, unsigned seed) {
    int ci = -1, ti = -1;
    const int tgt = legalTarget(w.balls);
    if (tgt < 0) return 0.0;
    for (size_t i = 0; i < w.balls.size(); ++i) {
        if (w.balls[i].type == BallType::Cue) ci = static_cast<int>(i);
        if (w.balls[i].id == tgt && !w.balls[i].pocketed)
            ti = static_cast<int>(i);
    }
    if (ci < 0 || ti < 0) return 0.0;
    const Vec3 cue = w.balls[ci].r, T = w.balls[ti].r;
    const double Rb = 2.0 * k::R;

    double best = 0.0;
    for (const Vec3& P : w.table.pockets()) {          // 6 direct aims only
        Vec3 dTP = P - T; dTP.y = 0.0; dTP = dTP.normalized();
        Vec3 ghost = T - dTP * Rb;
        Vec3 aim = ghost - cue; aim.y = 0.0; aim = aim.normalized();
        if (aim.dot(dTP) < std::cos(75.0 * 3.14159265 / 180.0)) continue;
        ShotEval e;
        e.shot = {aim, 2.4, 0.0, 0.0};
        e.targetId = tgt;
        best = std::max(best, evaluate(w, e, 6, seed));
        if (best > 0.97) break;
    }
    return best;
}

// One candidate's win prob. Rollouts only TALLY outcome fractions and
// capture one representative state per branch; the (expensive) continuation
// and opponent values are then evaluated ONCE per branch, not per rollout.
double candidateWin(const World& w, const ShotEval& e, int foulsOnMover,
                    int ply, int nRollouts, int beamK, unsigned seed) {
    std::mt19937 rng(seed * 2654435761u + 7u);
    std::normal_distribution<double> nAim(0.0, 0.009), nSpd(0.0, 0.05);

    int nWin = 0, nCont = 0, nMiss = 0, nFoul = 0, nLose = 0;
    World repCont, repMiss, repFoul;
    bool hC = false, hM = false, hF = false;

    for (int r = 0; r < nRollouts; ++r) {
        World ww = w;
        const int ci = cueIdx(ww.balls);
        const double th = nAim(rng), c = std::cos(th), s = std::sin(th);
        const Vec3 aim{e.shot.aim.x * c + e.shot.aim.z * s, 0.0,
                       -e.shot.aim.x * s + e.shot.aim.z * c};
        cueStrike(ww.balls[ci], aim, e.shot.speed * (1.0 + nSpd(rng)),
                  e.shot.a, e.shot.b);
        const int legalBefore = legalTarget(ww.balls);
        ShotOutcome o = simulateShot(ww);

        if (o.won) {
            ++nWin;
        } else if (o.foul != Foul::None) {
            if (foulsOnMover + 1 >= 3) ++nLose;        // 3rd consecutive
            else { ++nFoul; if (!hF) { repFoul = ww; hF = true; } }
        } else {
            bool potted = false;
            for (int id : o.pocketed) if (id == legalBefore) potted = true;
            if (potted) { ++nCont; if (!hC) { repCont = ww; hC = true; } }
            else        { ++nMiss; if (!hM) { repMiss = ww; hM = true; } }
        }
    }

    const double N = nRollouts;
    const double Vcont = hC ? posValue(repCont, seed + 1) : 0.0;
    double Vmiss = 0.0, Vfoul = 0.0;
    if (hM) {
        const double opp = (ply <= 0) ? posValue(repMiss, seed + 2)
                                      : moverWin(repMiss, 0, ply - 1,
                                                 nRollouts, beamK, seed + 2);
        Vmiss = 1.0 - opp;                            // turn passed
    }
    if (hF) {
        const double opp = (ply <= 0) ? posValue(repFoul, seed + 3)
                                      : moverWin(repFoul, 0, ply - 1,
                                                 nRollouts, beamK, seed + 3);
        Vfoul = 1.0 - std::min(1.0, opp + kBIH);      // opp ball-in-hand
    }
    return (nWin * 1.0 + nCont * Vcont + nMiss * Vmiss + nFoul * Vfoul +
            nLose * 0.0) /
           N;
}

double moverWin(const World& w, int foulsOnMover, int ply, int nRollouts,
                int beamK, unsigned seed) {
    std::vector<ShotEval> cands = candidateShots(w);
    for (ShotEval& s : safetyCandidates(w)) cands.push_back(s);
    if (cands.empty()) return 0.0;

    // Beam: rank by quick single-shot pot prob, keep top-K (+ all safeties,
    // since their pot prob is ~0 but their win value can be high).
    std::mt19937 rng(seed);
    for (ShotEval& e : cands)
        e.pPot = (e.kind == ShotKind::Safety)
                     ? 0.0
                     : evaluate(w, e, 3, seed ^ 0x9E3779B1u);
    std::sort(cands.begin(), cands.end(),
              [](const ShotEval& a, const ShotEval& b) {
                  return a.pPot > b.pPot;
              });
    std::vector<ShotEval> beam;
    for (ShotEval& e : cands) {
        if (static_cast<int>(beam.size()) < beamK ||
            e.kind == ShotKind::Safety)
            beam.push_back(e);
    }

    double best = 0.0;
    int i = 0;
    for (const ShotEval& e : beam) {
        double v = candidateWin(w, e, foulsOnMover, ply, nRollouts, beamK,
                                seed + 100u * (++i));
        best = std::max(best, v);
    }
    return best;
}

}  // namespace

WinPlan planWin(const World& w, int foulsOnMover, int nRollouts, int beamK,
                unsigned seed) {
    std::vector<ShotEval> cands = candidateShots(w);
    for (ShotEval& s : safetyCandidates(w)) cands.push_back(s);
    WinPlan best;
    if (cands.empty()) return best;

    for (ShotEval& e : cands)
        e.pPot = (e.kind == ShotKind::Safety)
                     ? 0.0
                     : evaluate(w, e, 3, seed ^ 0x9E3779B1u);
    std::sort(cands.begin(), cands.end(),
              [](const ShotEval& a, const ShotEval& b) {
                  return a.pPot > b.pPot;
              });
    std::vector<ShotEval> beam;
    for (ShotEval& e : cands)
        if (static_cast<int>(beam.size()) < beamK ||
            e.kind == ShotKind::Safety)
            beam.push_back(e);

    int i = 0;
    for (const ShotEval& e : beam) {
        double v = candidateWin(w, e, foulsOnMover, 2, nRollouts, beamK,
                                seed + 100u * (++i));
        if (v > best.winProb) {
            best.shot = e;
            best.winProb = v;
        }
    }
    return best;
}

}  // namespace cue
