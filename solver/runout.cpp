#include "solver/runout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "solver/difficulty.h"

namespace cue {
namespace {

constexpr double DEG = 3.14159265358979323846 / 180.0;
Vec3 planar(Vec3 v) { v.y = 0.0; return v; }

int cueIdx(const std::vector<Ball>& b) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].type == BallType::Cue) return (int)i;
    return -1;
}
int idxOfId(const std::vector<Ball>& b, int id) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].id == id && !b[i].pocketed) return (int)i;
    return -1;
}
Vec3 ghostOf(const Vec3& O, const Vec3& P) {
    return O - planar(P - O).normalized() * (2.0 * k::R);
}
bool blocked(const World& w, const Vec3& a, const Vec3& b, int x, int y) {
    Vec3 ab = planar(b - a);
    double L = ab.norm();
    if (L < 1e-9) return false;
    Vec3 d = ab / L;
    for (const Ball& o : w.balls) {
        if (o.pocketed || o.id == x || o.id == y) continue;
        if (o.type == BallType::Cue && (x == 0 || y == 0)) continue;
        Vec3 ao = planar(o.r - a);
        double t = std::max(0.0, std::min(L, ao.dot(d)));
        if (planar(o.r - (a + d * t)).norm() < 1.8 * k::R) return true;
    }
    return false;
}
double angBetween(const Vec3& u, const Vec3& v) {
    double c = planar(u).normalized().dot(planar(v).normalized());
    c = c < -1 ? -1 : (c > 1 ? 1 : c);
    return std::acos(c);
}
// Sorted (ascending id) non-pocketed object balls = the legal order.
std::vector<int> legalOrder(const World& w) {
    std::vector<int> ids;
    for (const Ball& b : w.balls)
        if (b.type != BallType::Cue && !b.pocketed) ids.push_back(b.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}
// Best table P(pot) for object `oi` from cue position `cue` (LOS-gated).
double bestPot(const World& w, const Vec3& cue, int oid) {
    const int oi = idxOfId(w.balls, oid);
    if (oi < 0) return 0.0;
    const Vec3 O = w.balls[oi].r;
    double best = 0.0;
    for (const Vec3& P : w.table.pockets()) {
        const Vec3 g = ghostOf(O, P);
        if (blocked(w, cue, g, 0, oid)) continue;
        if (blocked(w, O, P, oid, -999)) continue;
        const double alpha = angBetween(g - cue, P - O) / DEG;
        const double dCO = planar(g - cue).norm();
        const double dOP = planar(P - O).norm();
        best = std::max(best, difficulty().potProb(alpha, dCO, dOP));
    }
    return best;
}
// Cue-independent "openness" of object `oid` (any clear pocket, neutral
// angle) -- the coarse p2/p3 term of the ordered-9-ball value.
double openness(const World& w, int oid) {
    const int oi = idxOfId(w.balls, oid);
    if (oi < 0) return 0.0;
    const Vec3 O = w.balls[oi].r;
    double best = 0.0;
    for (const Vec3& P : w.table.pockets()) {
        if (blocked(w, O, P, oid, -999)) continue;
        best = std::max(best,
                        difficulty().potProb(30.0, 0.6,
                                             planar(P - O).norm()));
    }
    return best;
}

}  // namespace

double mobilityValue(const World& w) {
    const int ci = cueIdx(w.balls);
    std::vector<int> ord = legalOrder(w);
    if (ord.empty()) return 1.0;                 // rack cleared = ideal
    if (ci < 0) return 0.0;
    const Vec3 cue = w.balls[ci].r;
    // 9-ball adaptation of PickPocket/CueCard 1,0.33,0.15: ordered next-3
    // legal balls (you must shoot the lowest), p1 from the real cue.
    double p1 = bestPot(w, cue, ord[0]);
    double p2 = ord.size() > 1 ? openness(w, ord[1]) : 1.0;
    double p3 = ord.size() > 2 ? openness(w, ord[2]) : 1.0;
    return 1.0 * p1 + 0.33 * p2 + 0.15 * p3;
}

LeaveShot seedLeaveShot(const World& w, int targetId, int pocketIdx,
                        const Vec3& leave) {
    LeaveShot out;
    const int ci = cueIdx(w.balls);
    const int oi = idxOfId(w.balls, targetId);
    if (ci < 0 || oi < 0 || pocketIdx < 0 || pocketIdx > 5) return out;
    const Vec3 cue0 = w.balls[ci].r;
    const Vec3 O = w.balls[oi].r;
    const Vec3 P = w.table.pockets()[pocketIdx];
    const Vec3 dOP = planar(P - O).normalized();
    const Vec3 ghost = O - dOP * (2.0 * k::R);
    if (blocked(w, cue0, ghost, 0, targetId) ||
        blocked(w, O, P, targetId, -999))
        return out;
    if (angBetween(ghost - cue0, dOP) > 80.0 * DEG) return out;

    const Vec3 aim = planar(ghost - cue0).normalized();
    // Stun departure ~ component of the cue's approach perpendicular to the
    // line of centres; follow/draw blend it toward/away from "forward".
    const Vec3 fwd = aim;
    Vec3 tHat = fwd - dOP * fwd.dot(dOP);
    if (planar(tHat).norm() < 1e-6) tHat = {-dOP.z, 0, dOP.x};   // full hit
    tHat = planar(tHat).normalized();

    const Vec3 u = planar(leave - ghost);
    const Vec3 hHat = u.norm() > 1e-6 ? u.normalized() : tHat;

    // 1-D "solve" for the blend beta (NOT a blind 5-D search): pick the
    // post-contact heading closest to the desired one.
    double bestBeta = 0.0, bestAng = 1e9;
    for (int s = -6; s <= 6; ++s) {
        double beta = s / 6.0 * 1.6;
        Vec3 h = planar(tHat + fwd * beta);
        if (h.norm() < 1e-6) continue;
        double a = angBetween(h.normalized(), hHat);
        if (a < bestAng) { bestAng = a; bestBeta = beta; }
    }
    // Speed from the inverse rolling-distance law d = v^2/(2 mu_r g),
    // de-rated for collision energy loss and the stroke->ball gain.
    double vCue = std::sqrt(2.0 * k::MU_ROLL * k::G *
                            std::max(0.05, u.norm()));
    double speed = vCue / (0.70 * 1.52);
    speed = std::max(1.1, std::min(4.5, speed));
    double b = std::max(-0.45 * k::R,
                        std::min(0.45 * k::R, bestBeta / 1.6 * 0.45 * k::R));

    auto trial = [&](double sp, double bb) {
        World c = w;
        const int legalBefore = legalTarget(c.balls);
        cueStrike(c.balls[ci], aim, sp, 0.0, bb);
        ShotOutcome o = simulateShot(c);
        bool pot = false;
        for (int id : o.pocketed) if (id == legalBefore) pot = true;
        double err = pot ? planar(c.balls[ci].r - leave).norm() : 9.0;
        return std::make_pair(pot && o.foul == Foul::None, err);
    };

    // Seeded simulate-and-correct (converges fast: the seed is close).
    double step = 0.4;
    auto cur = trial(speed, b);
    out.potsTarget = cur.first;
    out.leaveErr = cur.second;
    out.shot = {{aim, speed, 0.0, b}, targetId, pocketIdx,
                ShotKind::Direct, -1, 0.0};
    for (int it = 0; it < 4; ++it) {
        double bestS = speed, bestB = b, bestE = out.leaveErr;
        bool bestP = out.potsTarget;
        for (double ds : {-step, 0.0, step})
            for (double db : {-0.15 * k::R, 0.0, 0.15 * k::R}) {
                double sp = std::max(1.1, std::min(4.5, speed + ds));
                double bb =
                    std::max(-0.45 * k::R, std::min(0.45 * k::R, b + db));
                auto r = trial(sp, bb);
                if (r.first && (!bestP || r.second < bestE)) {
                    bestP = true; bestE = r.second; bestS = sp; bestB = bb;
                }
            }
        speed = bestS; b = bestB;
        out.potsTarget = bestP; out.leaveErr = bestE;
        out.shot.shot.speed = speed; out.shot.shot.b = b;
        step *= 0.5;
    }
    return out;
}

// BR-1: per-candidate Monte-Carlo-over-noise.
// Mirrors solver/solver.cpp::evaluate's noise convention exactly so
// the deterministic-parallel result is bitwise consistent across
// callers: per-sample seed = baseSeed * 2654435761u + r, aim rotated
// in the planar xz by N(0, aimSigma), speed scaled by (1+N(0,
// speedRelSigma)).
McScore mcScore(const World& w, const ShotEval& e, int nSamples,
                unsigned baseSeed, double aimSigmaRad,
                double speedRelSigma) {
    McScore out{};
    out.samples = nSamples;
    if (nSamples <= 0 || e.targetId < 0) return out;
    const int ci = cueIdx(w.balls);
    if (ci < 0) return out;

    double sumPot = 0.0;
    double sumValue = 0.0;
    for (int r = 0; r < nSamples; ++r) {
        std::mt19937 rng(baseSeed * 2654435761u + (unsigned)r);
        std::normal_distribution<double> nA(0.0, aimSigmaRad);
        std::normal_distribution<double> nS(0.0, speedRelSigma);
        const double th = nA(rng), c = std::cos(th), s = std::sin(th);
        const Vec3 aim{e.shot.aim.x * c + e.shot.aim.z * s, 0.0,
                       -e.shot.aim.x * s + e.shot.aim.z * c};
        const double v = e.shot.speed * (1.0 + nS(rng));
        World ww = w;
        cueStrike(ww.balls[ci], aim, v, e.shot.a, e.shot.b);
        ShotOutcome o = simulateShot(ww);
        bool potted = false;
        for (int id : o.pocketed)
            if (id == e.targetId && o.foul == Foul::None) potted = true;
        if (potted) {
            sumPot += 1.0;
            // Skip mobility evaluation if the rack is cleared (e.g.,
            // the target was the 9 and the win flag is set) -- the
            // ordered legal-target check below would handle it but the
            // explicit early return is clearer.
            sumValue += mobilityValue(ww);
        }
    }
    out.pPotMC = sumPot / nSamples;
    out.valueMC = sumValue / nSamples;
    return out;
}

namespace {

// Feasible pockets for object `oid` from cue position `cue` (LOS-gated),
// with the table P(pot). Returns (pocketIdx, Ppot) pairs, P descending.
std::vector<std::pair<int, double>> feasiblePockets(const World& w,
                                                    const Vec3& cue,
                                                    int oid) {
    std::vector<std::pair<int, double>> v;
    const int oi = idxOfId(w.balls, oid);
    if (oi < 0) return v;
    const Vec3 O = w.balls[oi].r;
    const auto pk = w.table.pockets();
    for (int p = 0; p < 6; ++p) {
        const Vec3 g = ghostOf(O, pk[p]);
        if (blocked(w, cue, g, 0, oid)) continue;
        if (blocked(w, O, pk[p], oid, -999)) continue;
        const double a = angBetween(g - cue, pk[p] - O) / DEG;
        if (a > 78.0) continue;
        double pp = difficulty().potProb(a, planar(g - cue).norm(),
                                         planar(pk[p] - O).norm());
        if (pp > 0.03) v.push_back({p, pp});
    }
    std::sort(v.begin(), v.end(),
              [](auto& x, auto& y) { return x.second > y.second; });
    return v;
}

// Candidate leave zones that set up the next legal ball t2: behind its
// ghost toward each of its feasible pockets, multiple standoffs and
// fan-out angles. The denser zone set gives BR-1's MC ranker more
// noise-robust leaves to choose from -- otherwise the chain at shot 2
// is limited by whichever single zone the geometry happened to pick.
std::vector<Vec3> leaveZones(const World& w, int t2) {
    std::vector<Vec3> z;
    if (t2 < 0) return z;
    const int oi = idxOfId(w.balls, t2);
    if (oi < 0) return z;
    const Vec3 O2 = w.balls[oi].r;
    for (const Vec3& P2 : w.table.pockets()) {
        if (blocked(w, O2, P2, t2, -999)) continue;
        const Vec3 g2 = ghostOf(O2, P2);
        const Vec3 back = planar(g2 - O2).normalized();
        // Tangent direction in the table plane (perpendicular to back).
        const Vec3 tan{-back.z, 0.0, back.x};
        // Centre standoff at three depths plus one fan-out per depth --
        // 6 zones per feasible pocket. Capped to keep planRunOut cost
        // bounded under BR-1's MC-all-candidates ranking.
        for (double L : {0.20, 0.40}) {
            z.push_back(g2 + back * L);
            z.push_back(g2 + back * L + tan * (0.08 * L));
            z.push_back(g2 + back * L - tan * (0.08 * L));
        }
        if (z.size() >= 18) break;
    }
    return z;
}

}  // namespace

namespace {
bool g_useMc = false;
int g_mcSamples = 12;
double g_mcAimSigma = k::AIM_SIGMA;
double g_mcSpeedSigma = k::SPEED_SIGMA;
bool g_useRescue = false;
int g_rescueSamples = 16;
double g_rescueMinPot = 0.05;
// BR-3: noise-aware depth-2 recursion sample count. When >0, the
// depth-2 recursion samples c.after over K noisy executions of shot 1
// and averages the recursive nxt.value -- so the planner ranks shot 1
// by E[future chain value | NOISY shot-1 execution] rather than by
// "chain value if shot 1 executes perfectly." Default 0 (preserves
// the bit-exact noiseless-recursion behaviour for the 22-suite battery).
int g_deepSamples = 0;
// BR-4 (MCTS): rollout-based candidate scoring. When > 0, each shot-1
// candidate is scored by P(full-chain clear over K noisy rollouts) --
// the direct end-to-end estimator, not a heuristic. Inside the rollout,
// the post-shot-1 chain is executed greedily (planRunOut with
// depth=1, no MCTS recursion, no further BR-4) up to g_mctsDepth shots.
int g_mctsRollouts = 0;
int g_mctsDepth = 8;

// BR-2 helper: build Kick/Bank rescue candidates for legal target `t`
// by reusing solver/solver.cpp::candidateShots (which generates the
// rail-mirror geometry the run-out planner doesn't otherwise consider)
// and filtering to rail-first kinds. The Direct subset is excluded
// because the caller has already established (via feasiblePockets)
// that no direct LOS exists. Returns the raw shots; the caller is
// responsible for MC-scoring and picking the best.
std::vector<ShotEval> rescueCandidates(const World& w, int t) {
    std::vector<ShotEval> out;
    if (legalTarget(w.balls) != t) return out;     // sanity
    for (const ShotEval& e : candidateShots(w)) {
        if (e.targetId != t) continue;
        if (e.kind == ShotKind::Kick || e.kind == ShotKind::Bank)
            out.push_back(e);
    }
    return out;
}
}  // namespace

namespace {

// BR-4 helper: run K noisy rollouts of `shot1` from world w, continuing
// greedily up to `maxDepth` shots, return the clear rate. Inner planner
// is greedy (no BR-1/BR-2/BR-4) so cost stays bounded.
double mctsRolloutScore(const World& w, const ShotEval& shot1,
                        int K, int maxDepth, double aimSigma,
                        double speedSigma, unsigned baseSeed) {
    int cleared = 0;
    const int ciOuter = cueIdx(w.balls);
    if (ciOuter < 0) return 0.0;
    // Disable nested MCTS (would explode cost), keep BR-1 / BR-2
    // active so the rollout continuation matches what the harness
    // actually executes. The rollout cost stays bounded by
    // g_mctsDepth -- with BR-1 each inner planRunOut is ~150 ms,
    // dominated by candidate generation, not MC scoring.
    int savedMcts = g_mctsRollouts;
    g_mctsRollouts = 0;

    for (int r = 0; r < K; ++r) {
        std::mt19937 rng(baseSeed * 2654435761u + (unsigned)r);
        std::normal_distribution<double> nA(0.0, aimSigma);
        std::normal_distribution<double> nS(0.0, speedSigma);

        World wr = w;
        // Execute shot 1 under noise.
        const double th = nA(rng), co = std::cos(th), si = std::sin(th);
        const Vec3 aim{shot1.shot.aim.x * co + shot1.shot.aim.z * si, 0.0,
                       -shot1.shot.aim.x * si + shot1.shot.aim.z * co};
        const double v = shot1.shot.speed * (1.0 + nS(rng));
        cueStrike(wr.balls[ciOuter], aim, v, shot1.shot.a, shot1.shot.b);
        ShotOutcome o = simulateShot(wr);
        if (o.foul != Foul::None) continue;
        bool potted = false;
        for (int id : o.pocketed)
            if (id == shot1.targetId) potted = true;
        if (!potted) continue;
        if (o.won) { ++cleared; continue; }
        if (legalTarget(wr.balls) < 0) { ++cleared; continue; }

        // Greedy chain continuation. Each iteration: plan, execute
        // under noise, check pot. Bail on miss/foul/defensive.
        bool chained = true;
        for (int d = 1; d < maxDepth; ++d) {
            const int tgt = legalTarget(wr.balls);
            if (tgt < 0) break;
            RunOutPlan p = planRunOut(wr, 1, 3);
            if (p.defensive || p.shot.targetId < 0) { chained = false; break; }
            const int ci2 = cueIdx(wr.balls);
            const double th2 = nA(rng);
            const double co2 = std::cos(th2), si2 = std::sin(th2);
            const Vec3 aim2{p.shot.shot.aim.x * co2 + p.shot.shot.aim.z * si2,
                            0.0,
                            -p.shot.shot.aim.x * si2 +
                                p.shot.shot.aim.z * co2};
            const double v2 = p.shot.shot.speed * (1.0 + nS(rng));
            cueStrike(wr.balls[ci2], aim2, v2,
                      p.shot.shot.a, p.shot.shot.b);
            ShotOutcome o2 = simulateShot(wr);
            if (o2.foul != Foul::None) { chained = false; break; }
            if (o2.won) break;
            bool potted2 = false;
            for (int id : o2.pocketed) if (id == tgt) potted2 = true;
            if (!potted2) { chained = false; break; }
        }
        if (chained && legalTarget(wr.balls) < 0) ++cleared;
    }

    g_mctsRollouts = savedMcts;
    return (double)cleared / K;
}

}  // namespace

RunOutPlan planRunOut(const World& w, int depth, int beamK) {
    RunOutPlan out;
    const int ci = cueIdx(w.balls);
    std::vector<int> ord = legalOrder(w);
    if (ci < 0 || ord.empty()) { out.value = 1.0; return out; }
    const int t = ord[0];
    const int t2 = ord.size() > 1 ? ord[1] : -1;
    const Vec3 cue = w.balls[ci].r;

    auto pockets = feasiblePockets(w, cue, t);
    if (pockets.empty()) {
        // BR-2: rescue-shot capability. Generate Kick/Bank candidates
        // for the legal target, score by MC-over-noise, and pick the
        // best if any clears `g_rescueMinPot`. Mirrors the BR-1 seeding
        // convention (deterministic per shot identity).
        if (g_useRescue) {
            std::vector<ShotEval> raw = rescueCandidates(w, t);
            struct R { ShotEval shot; McScore mc; World after; };
            std::vector<R> rescues;
            for (size_t i = 0; i < raw.size(); ++i) {
                const ShotEval& e = raw[i];
                // Geometric pre-filter: the kick/bank candidates from
                // candidateShots use a mirror-aim heuristic; cushion
                // physics deviates from that, so most don't actually pot
                // even noiselessly. Validate noiseless pot first --
                // candidates that miss noiselessly are gamble shots
                // (relying on noise to randomly hit) and should not be
                // counted as rescues. This converts BR-2 from "any shot
                // with non-zero noisy pPot" to "actually-makeable shots
                // the lookup table happened to exclude."
                World after = w;
                cueStrike(after.balls[ci], e.shot.aim, e.shot.speed,
                          e.shot.a, e.shot.b);
                ShotOutcome o = simulateShot(after);
                bool noiselessPot = false;
                for (int id : o.pocketed)
                    if (id == t && o.foul == Foul::None) noiselessPot = true;
                if (!noiselessPot) continue;
                const unsigned seed = (unsigned)((t * 1009 +
                                                  e.pocket * 17 +
                                                  (e.rail + 1) * 41 +
                                                  (int)i * 31) | 1u);
                McScore mc = mcScore(w, e, g_rescueSamples, seed,
                                     g_mcAimSigma, g_mcSpeedSigma);
                if (mc.pPotMC < g_rescueMinPot) continue;
                rescues.push_back({e, mc, after});
            }
            if (!rescues.empty()) {
                int bestI = 0;
                double bestV = -1.0;
                for (size_t i = 0; i < rescues.size(); ++i) {
                    const double v =
                        rescues[i].mc.pPotMC *
                        mobilityValue(rescues[i].after);
                    if (v > bestV) { bestV = v; bestI = (int)i; }
                }
                out.shot = rescues[bestI].shot;
                out.value = bestV;
                out.defensive = false;
                return out;
            }
        }
        out.defensive = true;
        out.defCause = DefensiveCause::NoLOS;
        return out;
    }

    std::vector<Vec3> zones = leaveZones(w, t2);
    if (zones.empty()) zones.push_back(w.balls[idxOfId(w.balls, t)].r);

    // Level 1: build candidates (pot t via pocket, leave for t2), score by
    // table P(pot) * mobility of the modal post-shot state.
    struct Cand { ShotEval shot; World after; double pPot, lvl1; };
    std::vector<Cand> cands;
    for (auto& pr : pockets) {
        for (const Vec3& z : zones) {
            LeaveShot ls = seedLeaveShot(w, t, pr.first, z);
            if (!ls.potsTarget) continue;
            World after = w;
            cueStrike(after.balls[ci], ls.shot.shot.aim,
                      ls.shot.shot.speed, ls.shot.shot.a, ls.shot.shot.b);
            simulateShot(after);
            Cand c;
            c.shot = ls.shot;
            c.after = after;
            c.pPot = pr.second;
            c.lvl1 = pr.second * mobilityValue(after);
            cands.push_back(c);
        }
    }
    if (cands.empty()) {
        out.defensive = true;
        out.defCause = DefensiveCause::CandsEmpty;
        return out;
    }

    // BR-1: MC-score ALL candidates BEFORE the top-K truncation.
    // Rationale: the chain-failure-at-shot-2 diagnostic showed that
    // noise-robust leaves are the limiting factor. The lookup table
    // ranks by P(pot) * mobility(noiseless_leave) -- it can demote a
    // noise-robust shot to rank 4 if its noiseless leave happens to be
    // marginally worse than a fragile leave. With the previous "top-K
    // by lookup, then MC re-rank" order, that noise-robust candidate
    // never reached the MC stage. Scoring all candidates first means
    // the survivors of the beamK cut are the MC-best, not the
    // lookup-best -- exactly the candidates whose chain continues
    // under noise.
    if (g_mctsRollouts > 0) {
        // BR-4: rank candidates by end-to-end rollout clear rate. The
        // heuristic ranker (mc.valueMC) and the depth-2 recursion are
        // bypassed -- the rollouts ARE the planner. Pre-filter to the
        // top `2*beamK` lookup candidates so MCTS cost stays bounded;
        // those that survive get the full rollout treatment. Inner
        // rollouts use greedy continuation; chain cost bounded by
        // g_mctsDepth.
        std::sort(cands.begin(), cands.end(),
                  [](const Cand& a, const Cand& b) {
                      return a.lvl1 > b.lvl1;
                  });
        const int prefilter = std::max(beamK, 2 * beamK);
        if ((int)cands.size() > prefilter) cands.resize(prefilter);
        // Save lookup-based lvl1 BEFORE overwriting with MCTS scores so
        // we can fall back if all MCTS estimates come back zero (low-K
        // sampling noise; the "best lookup candidate is still the best
        // guess" assumption is safer than bailing defensively).
        std::vector<double> lookupLvl1;
        lookupLvl1.reserve(cands.size());
        for (auto& c : cands) lookupLvl1.push_back(c.lvl1);

        double maxClear = 0.0;
        for (size_t i = 0; i < cands.size(); ++i) {
            Cand& c = cands[i];
            const unsigned seed = (unsigned)((c.shot.targetId * 1009 +
                                              c.shot.pocket * 17 +
                                              (int)i * 31) | 1u);
            const double clearRate =
                mctsRolloutScore(w, c.shot, g_mctsRollouts, g_mctsDepth,
                                 g_mcAimSigma, g_mcSpeedSigma, seed);
            c.pPot = clearRate;
            c.lvl1 = clearRate;
            if (clearRate > maxClear) maxClear = clearRate;
        }
        // MCTS sampling-noise fallback: with K=12 and per-shot chain
        // success ~10 %, P(all K rollouts fail) ~ 28 % per candidate.
        // If every candidate's rollouts come back zero, that's almost
        // certainly bad luck rather than an actually impossible state.
        // Restore the lookup lvl1 so the planner picks the best-by-
        // lookup candidate instead of bailing LowValue.
        if (maxClear < 1e-4) {
            for (size_t i = 0; i < cands.size(); ++i)
                cands[i].lvl1 = lookupLvl1[i];
        }
    } else if (g_useMc) {
        for (size_t i = 0; i < cands.size(); ++i) {
            Cand& c = cands[i];
            const unsigned seed = (unsigned)((c.shot.targetId * 1009 +
                                              c.shot.pocket * 17 +
                                              (int)i * 31) | 1u);
            McScore mc = mcScore(w, c.shot, g_mcSamples, seed,
                                 g_mcAimSigma, g_mcSpeedSigma);
            c.pPot = mc.pPotMC;
            c.lvl1 = mc.valueMC;
        }
    }
    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.lvl1 > b.lvl1; });
    if ((int)cands.size() > beamK) cands.resize(beamK);

    // Level 2: expand the beam one more ball. Default behaviour: recurse
    // on the NOISELESS c.after (preserves the 22-suite regression). BR-3
    // (g_deepSamples > 0): sample c.after over noisy shot-1 executions,
    // recurse on each, average the recursive nxt.value. This gives the
    // planner E[future chain value | noisy shot 1] -- the correct ranker
    // for the chain-survival-under-noise objective the harness measures.
    double best = -1.0;
    for (Cand& c : cands) {
        double v;
        // MCTS rollout score already encodes the full-chain value;
        // skip the depth-2 recursion (it would either duplicate the
        // rollout work or, worse, invoke nested MCTS on c.after).
        if (g_mctsRollouts > 0 || depth <= 1 ||
            legalTarget(c.after.balls) < 0) {
            v = c.lvl1;
        } else if (g_deepSamples > 0) {
            double sum = 0.0;
            int hits = 0;
            for (int s = 0; s < g_deepSamples; ++s) {
                const unsigned dseed =
                    (unsigned)((c.shot.targetId * 1009 +
                                c.shot.pocket * 17 +
                                (int)s * 2654435761u) | 1u);
                std::mt19937 rng(dseed);
                std::normal_distribution<double> nA(0.0, g_mcAimSigma);
                std::normal_distribution<double> nS(0.0, g_mcSpeedSigma);
                const double th = nA(rng), co = std::cos(th), si = std::sin(th);
                const Vec3 aimN{c.shot.shot.aim.x * co + c.shot.shot.aim.z * si,
                                0.0,
                                -c.shot.shot.aim.x * si +
                                    c.shot.shot.aim.z * co};
                const double vN = c.shot.shot.speed * (1.0 + nS(rng));
                World noisy = w;
                cueStrike(noisy.balls[ci], aimN, vN,
                          c.shot.shot.a, c.shot.shot.b);
                ShotOutcome o = simulateShot(noisy);
                bool potted = false;
                for (int id : o.pocketed)
                    if (id == c.shot.targetId &&
                        o.foul == Foul::None) potted = true;
                if (!potted) continue;
                if (legalTarget(noisy.balls) < 0) {
                    sum += 1.0;
                } else {
                    RunOutPlan nxt = planRunOut(noisy, depth - 1, beamK);
                    sum += nxt.defensive ? 0.0 : nxt.value;
                }
                ++hits;
            }
            v = (hits > 0) ? (sum / g_deepSamples) : 0.0;
        } else {
            RunOutPlan nxt = planRunOut(c.after, depth - 1, beamK);
            v = c.pPot * (legalTarget(c.after.balls) < 0
                              ? 1.0
                              : (nxt.defensive ? 0.0 : nxt.value));
        }
        if (v > best) { best = v; out.shot = c.shot; out.value = v; }
    }
    if (best <= 1e-4) {
        out.defensive = true;
        out.defCause = DefensiveCause::LowValue;
    }
    return out;
}

void setUseMcScoring(bool on, int nSamples, double aimSigma,
                     double speedSigma) {
    g_useMc = on;
    g_mcSamples = std::max(2, nSamples);
    g_mcAimSigma = std::max(0.0, aimSigma);
    g_mcSpeedSigma = std::max(0.0, speedSigma);
}

void setDeepSamples(int nSamples) {
    g_deepSamples = std::max(0, nSamples);
}

void setMctsRollouts(int nRollouts, int nDepth) {
    g_mctsRollouts = std::max(0, nRollouts);
    g_mctsDepth = std::max(2, nDepth);
}

void setUseRescueShots(bool on, int nSamples, double minPotMC) {
    g_useRescue = on;
    g_rescueSamples = std::max(2, nSamples);
    g_rescueMinPot = std::max(0.0, minPotMC);
}

}  // namespace cue
