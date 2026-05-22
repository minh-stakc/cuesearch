#pragma once
// RO-2: canonical mobility value over the difficulty table + an
// inverse-physics leave generator. The literature's value function
// (PickPocket/CueCard: 1*p1 + 0.33*p2 + 0.15*p3) adapted to 9-ball's
// ORDERED constraint: p1..p3 = best pot prob of the next THREE legal
// balls (lowest first), each from the difficulty table. The leave
// generator SEEDS (speed, vertical-spin) from the tangent/30/trisect
// model + d=v^2/(2 mu_r g) -- so we compute the leave instead of blindly
// searching (POS-a's failure mode), then a few simulate-and-correct
// steps. Additive: planRunout/CP7-8/shape untouched.
#include "solver/solver.h"

namespace cue {

// Mobility value of a state for the side to move (0..~1.48). Fast: table
// lookups + line-of-sight, no live MC. difficulty() must be built first.
double mobilityValue(const World& w);

// A shot that pots `targetId` via `pocketIdx` and leaves the cue near
// `leave`. Seeded from the inverse cue-ball-control model, then refined
// by a few noiseless simulate-and-correct steps. potsTarget=false if no
// makeable shot was found.
struct LeaveShot {
    ShotEval shot;
    bool potsTarget = false;
    double leaveErr = 1e9;   // achieved |cue_rest - leave| (m)
};
LeaveShot seedLeaveShot(const World& w, int targetId, int pocketIdx,
                        const Vec3& leave);

// RO-3: two-level run-out search (CueCard structure). Goal-directed
// candidates (pot the legal ball into each feasible pocket, leaving the
// cue at zones that set up the NEXT ball -- via seedLeaveShot), scored by
// table P(pot) * mobilityValue (fast, no live MC); top-k expanded one
// more level; defensive flag when nothing makeable. Deterministic.
// Sub-cause when defensive: which branch in planRunOut bailed.
// Tells callers (B&R harness) which named lever is the right next move:
//   NoLOS     -> positional / break-model issue, BR-1 (MC scoring) can't help
//   CandsEmpty-> geometric candidates exist but noiseless seed couldn't pot;
//                BR-1's MC might still find them playable
//   LowValue  -> candidates exist, all score near zero; BR-1's natural target
enum class DefensiveCause { None, NoLOS, CandsEmpty, LowValue };

struct RunOutPlan {
    ShotEval shot;
    double value = 0.0;     // chained P(pot)*mobility, depth-limited
    bool defensive = false; // no makeable shot -> caller should play safe
    DefensiveCause defCause = DefensiveCause::None;
};

// BR-1: per-candidate Monte-Carlo-over-noise score. Replaces the
// lookup-table P(pot) * mobility(modal_leave) with an MC estimate of
// E[pot * mobility(after) | calibrated execution noise]. Per pre-reg
// docs/BREAK_AND_RUN.md, this is the single biggest missing piece per
// the Smith / CueCard literature.
struct McScore {
    double pPotMC;    // P(target pocketed legally | noise)
    double valueMC;   // E[indicator(pot) * mobility(after_noisy)]
    int samples;      // K
};
McScore mcScore(const World& w, const ShotEval& e, int nSamples,
                unsigned baseSeed,
                double aimSigmaRad = k::AIM_SIGMA,
                double speedRelSigma = k::SPEED_SIGMA);
RunOutPlan planRunOut(const World& w, int depth = 2, int beamK = 3);

// BR-1 toggle. When true, planRunOut MC-scores ALL candidates over the
// configured execution-noise sigmas (defaults to k::AIM_SIGMA /
// k::SPEED_SIGMA), then ranks them by mc.valueMC. Default OFF so the
// 22-suite regression battery stays bit-exact; B&R harness flips it on.
// Pass execution-matched sigmas via the optional aim/speed arguments so
// the planner's noise model matches the harness's execution noise --
// otherwise the chain breaks at shot 2 because the planner ranked by a
// noise distribution different from the one actually being run.
void setUseMcScoring(bool on, int nSamples = 12,
                     double aimSigma = k::AIM_SIGMA,
                     double speedSigma = k::SPEED_SIGMA);

// BR-2: rescue-shot capability for NoLOS positions. When the legal
// target has no direct LOS to any pocket (cue->ghost or target->pocket
// blocked for every pocket), instead of bailing immediately, expand the
// search to include KICK (cue rails first) and BANK (target rails first)
// candidates from solver/solver.cpp::candidateShots. Each is scored by
// mcScore -- the noiseless lookup table doesn't model rail-first shots.
// If any candidate's pPotMC exceeds minPotMC, the highest-value one is
// returned and out.defensive stays false. Otherwise NoLOS is preserved.
//
// Pre-reg context (docs/BREAK_AND_RUN.md, Baseline 1b): NoLOS is 38 %
// of B&R failures and explicitly named as BR-1-unaddressable / "the
// next named lever" in the diagnosis. Default OFF; B&R harness opts in
// via --br2.
void setUseRescueShots(bool on, int nSamples = 16,
                       double minPotMC = 0.05);

// BR-3: noise-aware depth-2 recursion. The default depth-2 path
// recurses on the NOISELESS post-shot state, so the planner ranks
// shot 1 by "shot 1 leaves a great noiseless shot 2." Under any
// execution noise, the cue lands at a slightly different position
// than the noiseless target -- the planner's chosen shot 2 may no
// longer work. Setting nSamples > 0 makes the recursion sample K
// noisy executions of shot 1, recurse on each, and average the
// recursive nxt.value -- ranking shot 1 by E[future chain value |
// noisy shot 1]. Default 0 (off), preserves the 22-suite regression.
void setDeepSamples(int nSamples);

// BR-4 (MCTS): for each shot-1 candidate, run K rollouts (execute the
// candidate under noise, then continue greedy until rack clears or
// chain fails). Score by clear rate. Replaces the mobility-heuristic
// ranker with a direct end-to-end chain-success estimator. When
// nRollouts > 0, the planner uses MCTS scoring instead of mc.valueMC
// for ranking; nDepth caps the rollout shot count. Default 0 (off).
void setMctsRollouts(int nRollouts, int nDepth = 8);

}  // namespace cue
