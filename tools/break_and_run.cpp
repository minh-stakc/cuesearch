// Break-and-run (B&R) diagnostic harness.
//
// Purpose: per the pre-registration in docs/BREAK_AND_RUN.md, measure
// the engine's break-and-run rate end-to-end (break the rack, then run
// out 1..9 in order under calibrated execution noise) and emit the
// PLATEAU DIAGNOSIS the pre-reg requires: legal-break rate, 9-on-break
// rate, per-shot chain length, terminal-cause distribution.
//
// This is a DIAGNOSTIC tool, NOT the pre-registered final gate.
// The pre-reg's BR-final gate is run ONCE after all named architectural
// changes (BR-1 MC-over-noise, BR-2 break model validation, BR-3 finer
// table / deeper search) are in place; commit messages will mark that
// run explicitly as the BR-final gate. Measurements made by this tool
// before that point are baselines / diagnostics / "is the harness wired
// right" -- they do NOT consume the binding stop condition.
//
// The break choice is LOCKED up front (textbook controlled break: cue
// at the head string centre, apex aim, ~10 m/s, light follow). This is
// NOT the golden_break winner (which optimises P(9-on-break), i.e.
// anti-B&R) -- they are separate efforts. Tuning the break parameters
// to make B&R look better is p-hacking on a different axis and is
// forbidden by the same pre-reg discipline.
//
// CLI:
//   break_and_run --trials N --shot-cap K --seed-base S
//   break_and_run --ball-in-hand    (skip break -- sanity-check the
//                                    run-out half against RO-4's
//                                    ball-in-hand 0/24)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "engine/world.h"
#include "solver/difficulty.h"
#include "solver/runout.h"

using namespace cue;

namespace {

enum class TerminalCause {
    Cleared,           // ran the rack 1..9 legally
    GoldenBreak,       // 9 potted on a LEGAL break (counts as B&R)
    IllegalBreak,      // scratch / no rail-or-pot on break
    Miss,              // missed the legal target during run-out
    Scratch,           // scratched during run-out
    WrongBallFirst,    // hit non-target first during run-out
    NoContact,         // no ball contact during run-out
    SafetyBail,        // planRunOut returned defensive
    ShotCap,           // hit shot cap without resolving
};

const char* causeName(TerminalCause c) {
    switch (c) {
        case TerminalCause::Cleared:        return "Cleared";
        case TerminalCause::GoldenBreak:    return "GoldenBreak";
        case TerminalCause::IllegalBreak:   return "IllegalBreak";
        case TerminalCause::Miss:           return "Miss";
        case TerminalCause::Scratch:        return "Scratch";
        case TerminalCause::WrongBallFirst: return "WrongBallFirst";
        case TerminalCause::NoContact:      return "NoContact";
        case TerminalCause::SafetyBail:     return "SafetyBail";
        case TerminalCause::ShotCap:        return "ShotCap";
    }
    return "?";
}

struct Trial {
    unsigned seed;
    bool legalBreak;
    bool nineOnBreak;
    int  ballsPottedOnBreak;
    int  chainLength;       // balls potted after break (in legal order)
    TerminalCause terminal;
    bool isBR() const {
        return terminal == TerminalCause::Cleared ||
               terminal == TerminalCause::GoldenBreak;
    }
};

// 9-ball rack with positional jitter; SAME slot pattern as
// golden_break.cpp / trace_shot.cpp rackForSearch so the audit trail
// across tools is consistent. Cue placed at (0, R, z0); caller
// overrides X/Z.
World rackWithJitter(std::mt19937& rng) {
    World w;
    const double R = k::R;
    const double s = 2.0 * R * 1.015, pitch = s * 0.86602540378;
    const double fx = 0.75 * w.table.xMax, z0 = 0.5 * w.table.zMax;
    std::uniform_real_distribution<double> J(-3e-4, 3e-4);
    struct Slot { double x, z; };
    Slot slot[9] = {
        {fx, z0},
        {fx + pitch,     z0 - s / 2}, {fx + pitch,     z0 + s / 2},
        {fx + 2 * pitch, z0 - s},     {fx + 2 * pitch, z0},
        {fx + 2 * pitch, z0 + s},
        {fx + 3 * pitch, z0 - s / 2}, {fx + 3 * pitch, z0 + s / 2},
        {fx + 4 * pitch, z0},
    };
    std::vector<int> rest = {2, 3, 4, 5, 6, 7, 8};
    std::shuffle(rest.begin(), rest.end(), rng);
    int id[9]; id[0] = 1; id[4] = 9;
    for (int k = 0, r = 0; k < 9; ++k)
        if (k != 0 && k != 4) id[k] = rest[r++];
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = {0.0, R, z0}; w.balls.push_back(cue);
    for (int k = 0; k < 9; ++k) {
        Ball b; b.type = BallType::Object; b.id = id[k];
        b.r = {slot[k].x + J(rng), R, slot[k].z + J(rng)};
        w.balls.push_back(b);
    }
    return w;
}

int cueIdx(const World& w) {
    for (size_t i = 0; i < w.balls.size(); ++i)
        if (w.balls[i].type == BallType::Cue) return static_cast<int>(i);
    return -1;
}

// LOCKED break params (textbook controlled): cue at head-string area,
// centre-of-width, dead-on apex, ~10 m/s, light follow, no english.
// Hard-coded here so the harness can't drift.
struct BreakSpec {
    double cueX  = 0.30;          // m, in the kitchen
    double cueZ  = 0.635;         // m, table centre width
    double aimDz = 0.0;            // dead-on apex
    double speed = 10.0;           // m/s, controlled
    double a     = 0.0;            // no side
    double b     = 0.3 * k::R;     // light follow
};

// One legal-break test under calibrated noise.
struct BreakOutcome {
    bool legal;
    bool nineOnBreak;
    int  ballsPotted;
    bool cueScratched;
};

BreakOutcome doBreak(World& w, const BreakSpec& bs, std::mt19937& rng,
                     double aimSigma, double speedSigma) {
    const int ci = cueIdx(w);
    w.balls[ci].r.x = bs.cueX;
    w.balls[ci].r.z = bs.cueZ;
    Vec3 apex = w.balls[ci].r;
    for (const Ball& b : w.balls) if (b.id == 1) apex = b.r;
    apex.z += bs.aimDz;
    Vec3 aim = apex - w.balls[ci].r; aim.y = 0; aim = aim.normalized();
    std::normal_distribution<double> nA(0.0, aimSigma);
    std::normal_distribution<double> nS(0.0, speedSigma);
    const double th = nA(rng), c = std::cos(th), sn = std::sin(th);
    aim = {aim.x * c + aim.z * sn, 0.0, -aim.x * sn + aim.z * c};
    const double v = bs.speed * (1.0 + nS(rng));
    cueStrike(w.balls[ci], aim, v, bs.a, bs.b);
    ShotOutcome o = simulateShot(w);
    BreakOutcome r;
    // Approximate legal-break: no foul AND (>=1 ball pocketed OR cue
    // hit the 1 first). The exact WPA "drive >=4 to a rail when nothing
    // pocketed" needs rail-contact instrumentation; at this speed any
    // legal break drives the rack to the rails -- the approximation is
    // tight in practice. Documented limitation.
    r.legal = (o.foul == Foul::None) &&
              (!o.pocketed.empty() || o.firstContact == 1);
    r.nineOnBreak = o.won && r.legal;
    r.ballsPotted = (int)o.pocketed.size();
    r.cueScratched = o.cueScratched;
    return r;
}

// Ball-in-hand placement for the 1: maximises planRunOut value over a
// small probe grid behind the 1 toward each pocket -- same logic as
// test_runout_ro4.cpp::ballInHand so the run-out half of this harness
// is comparable to RO-4.
void placeBallInHand(World& w) {
    const int ci = cueIdx(w);
    Vec3 O;
    for (const Ball& b : w.balls) if (b.id == 1) O = b.r;
    // Sensible default if no probe placement returns non-defensive:
    // mid-kitchen on the head string axis, matching RO-4's cleanRack.
    Vec3 bp{0.40, k::R, 0.63};
    double best = -1.0;
    for (const Vec3& P : w.table.pockets()) {
        Vec3 d = P - O; d.y = 0;
        if (d.norm() < 1e-6) continue;
        d = d.normalized();
        for (double L : {0.30, 0.45, 0.65}) {
            Vec3 pos = O - d * L; pos.y = k::R;
            if (pos.x < w.table.cxMin() || pos.x > w.table.cxMax() ||
                pos.z < w.table.czMin() || pos.z > w.table.czMax())
                continue;
            World t = w; t.balls[ci].r = pos;
            RunOutPlan rp = planRunOut(t, 2, 3);
            if (!rp.defensive && rp.value > best) {
                best = rp.value; bp = pos;
            }
        }
    }
    w.balls[ci].r = bp;
}

}  // namespace

int main(int argc, char** argv) {
    int TRIALS = 200;
    int SHOT_CAP = 12;
    unsigned SEED_BASE = 7000u;
    bool ballInHand = false;
    bool verbose = false;
    bool useBr1 = false;
    int br1Samples = 12;
    bool useBr2 = false;
    int br2Samples = 16;
    double br2MinPot = 0.05;
    int deepSamples = 0;
    int mctsRollouts = 0;
    int mctsDepth = 8;
    double execAimSigma = k::AIM_SIGMA;
    double execSpeedSigma = k::SPEED_SIGMA;
    double brkAimSigma = k::AIM_SIGMA;
    double brkSpeedSigma = k::SPEED_SIGMA;
    // Break parameter overrides (defaults match the LOCKED textbook break).
    double brkCueX  = 0.30;
    double brkCueZ  = 0.635;
    double brkAimDz = 0.0;
    double brkSpeed = 10.0;
    double brkA     = 0.0;
    double brkB     = 0.3 * k::R;
    // Planner depth/beam (default matches the existing harness).
    int planDepth = 2;
    int planBeamK = 3;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--trials" && i + 1 < argc)
            TRIALS = std::stoi(argv[++i]);
        else if (a == "--shot-cap" && i + 1 < argc)
            SHOT_CAP = std::stoi(argv[++i]);
        else if (a == "--seed-base" && i + 1 < argc)
            SEED_BASE = (unsigned)std::stoul(argv[++i]);
        else if (a == "--ball-in-hand") ballInHand = true;
        else if (a == "-v" || a == "--verbose") verbose = true;
        else if (a == "--br1") useBr1 = true;
        else if (a == "--br1-samples" && i + 1 < argc)
            br1Samples = std::stoi(argv[++i]);
        else if (a == "--br2") useBr2 = true;
        else if (a == "--br2-samples" && i + 1 < argc)
            br2Samples = std::stoi(argv[++i]);
        else if (a == "--br2-min-pot" && i + 1 < argc)
            br2MinPot = std::stod(argv[++i]);
        else if (a == "--aim-sigma" && i + 1 < argc)
            execAimSigma = std::stod(argv[++i]);
        else if (a == "--speed-sigma" && i + 1 < argc)
            execSpeedSigma = std::stod(argv[++i]);
        else if (a == "--break-cuex" && i + 1 < argc)
            brkCueX = std::stod(argv[++i]);
        else if (a == "--break-cuez" && i + 1 < argc)
            brkCueZ = std::stod(argv[++i]);
        else if (a == "--break-aim-dz" && i + 1 < argc)
            brkAimDz = std::stod(argv[++i]);
        else if (a == "--break-speed" && i + 1 < argc)
            brkSpeed = std::stod(argv[++i]);
        else if (a == "--break-side" && i + 1 < argc)
            brkA = std::stod(argv[++i]);
        else if (a == "--break-follow" && i + 1 < argc)
            brkB = std::stod(argv[++i]) * k::R;
        else if (a == "--depth" && i + 1 < argc)
            planDepth = std::stoi(argv[++i]);
        else if (a == "--beam" && i + 1 < argc)
            planBeamK = std::stoi(argv[++i]);
        else if (a == "--break-aim-sigma" && i + 1 < argc)
            brkAimSigma = std::stod(argv[++i]);
        else if (a == "--break-speed-sigma" && i + 1 < argc)
            brkSpeedSigma = std::stod(argv[++i]);
        else if (a == "--deep" && i + 1 < argc)
            deepSamples = std::stoi(argv[++i]);
        else if (a == "--mcts" && i + 1 < argc)
            mctsRollouts = std::stoi(argv[++i]);
        else if (a == "--mcts-depth" && i + 1 < argc)
            mctsDepth = std::stoi(argv[++i]);
    }
    // Match planner's MC noise to execution noise so the planner's
    // ranking of "noise-robust" candidates uses the correct distribution.
    setUseMcScoring(useBr1, br1Samples, execAimSigma, execSpeedSigma);
    setUseRescueShots(useBr2, br2Samples, br2MinPot);
    setDeepSamples(deepSamples);
    setMctsRollouts(mctsRollouts, mctsDepth);

    // Production-grade difficulty table (mirrors RO-4); built once.
    difficultyMut().buildOrLoad("difficulty_br.bin", 80);

    BreakSpec brk;
    brk.cueX  = brkCueX;
    brk.cueZ  = brkCueZ;
    brk.aimDz = brkAimDz;
    brk.speed = brkSpeed;
    brk.a     = brkA;
    brk.b     = brkB;
    std::vector<Trial> trials;
    trials.reserve(TRIALS);
    int cleared = 0, golden = 0, illegal = 0, scratch = 0, miss = 0;
    int wrongFirst = 0, noContact = 0, bail = 0, capped = 0;
    int bailNoLOS = 0, bailCandsEmpty = 0, bailLowValue = 0;
    int legalBreaks = 0;
    int totalBallsOnBreak = 0;
    std::vector<int> chainHist(10, 0);

    for (int t = 0; t < TRIALS; ++t) {
        std::mt19937 rng(SEED_BASE + (unsigned)t);
        World w = rackWithJitter(rng);
        Trial tr;
        tr.seed = SEED_BASE + (unsigned)t;
        tr.legalBreak = true;
        tr.nineOnBreak = false;
        tr.ballsPottedOnBreak = 0;
        tr.chainLength = 0;

        if (ballInHand) {
            placeBallInHand(w);
            if (verbose && t < 3) {
                std::fprintf(stderr, "trial %d after placeBallInHand: "
                             "cue at (%.3f, %.3f), 1 at (%.3f, %.3f)\n",
                             t, w.balls[cueIdx(w)].r.x,
                             w.balls[cueIdx(w)].r.z,
                             w.balls[1].r.x, w.balls[1].r.z);
                RunOutPlan p0 = planRunOut(w, planDepth, planBeamK);
                std::fprintf(stderr,
                    "  planRunOut: defensive=%d target=%d value=%.4f\n",
                    p0.defensive ? 1 : 0, p0.shot.targetId, p0.value);
            }
        } else {
            BreakOutcome bo = doBreak(w, brk, rng, brkAimSigma,
                                      brkSpeedSigma);
            tr.legalBreak = bo.legal;
            tr.nineOnBreak = bo.nineOnBreak;
            tr.ballsPottedOnBreak = bo.ballsPotted;
            totalBallsOnBreak += bo.ballsPotted;
            if (bo.nineOnBreak) {
                tr.terminal = TerminalCause::GoldenBreak;
                ++golden;
                ++legalBreaks;
                trials.push_back(tr);
                continue;
            }
            if (!bo.legal) {
                tr.terminal = TerminalCause::IllegalBreak;
                ++illegal;
                trials.push_back(tr);
                continue;
            }
            ++legalBreaks;
        }

        // Run-out loop: planRunOut + greedy execution with calibrated
        // execution noise. Stop on rack clear, miss, scratch, foul, or
        // shot cap.
        const int ci = cueIdx(w);
        std::normal_distribution<double> nA(0.0, execAimSigma);
        std::normal_distribution<double> nS(0.0, execSpeedSigma);
        bool resolved = false;
        for (int s = 0; s < SHOT_CAP; ++s) {
            const int tgt = legalTarget(w.balls);
            if (tgt < 0) { tr.terminal = TerminalCause::Cleared;
                           ++cleared; resolved = true; break; }
            // MCTS only on the FIRST shot (most chain length to plan
            // through). Subsequent shots use the regular planner --
            // by then the chain is shorter and the lookup heuristic
            // is good enough. Saves ~7x compute per trial.
            if (s == 0 && mctsRollouts > 0)
                setMctsRollouts(mctsRollouts, mctsDepth);
            else
                setMctsRollouts(0, mctsDepth);
            RunOutPlan p = planRunOut(w, planDepth, planBeamK);
            if (p.defensive || p.shot.targetId < 0) {
                tr.terminal = TerminalCause::SafetyBail;
                ++bail;
                if (p.defCause == DefensiveCause::NoLOS)      ++bailNoLOS;
                else if (p.defCause == DefensiveCause::CandsEmpty) ++bailCandsEmpty;
                else if (p.defCause == DefensiveCause::LowValue)   ++bailLowValue;
                resolved = true; break;
            }
            const double th = nA(rng), c = std::cos(th), sn = std::sin(th);
            Vec3 aim{p.shot.shot.aim.x * c + p.shot.shot.aim.z * sn, 0.0,
                     -p.shot.shot.aim.x * sn + p.shot.shot.aim.z * c};
            cueStrike(w.balls[ci], aim,
                      p.shot.shot.speed * (1.0 + nS(rng)),
                      p.shot.shot.a, p.shot.shot.b);
            ShotOutcome o = simulateShot(w);
            if (o.won && o.foul == Foul::None) {
                tr.terminal = TerminalCause::Cleared;
                ++cleared; resolved = true;
                ++tr.chainLength;
                break;
            }
            if (o.foul == Foul::Scratch) {
                tr.terminal = TerminalCause::Scratch;
                ++scratch; resolved = true; break;
            }
            if (o.foul == Foul::WrongBallFirst) {
                tr.terminal = TerminalCause::WrongBallFirst;
                ++wrongFirst; resolved = true; break;
            }
            if (o.foul == Foul::NoContact) {
                tr.terminal = TerminalCause::NoContact;
                ++noContact; resolved = true; break;
            }
            bool pottedTgt = false;
            for (int id : o.pocketed) if (id == tgt) pottedTgt = true;
            if (!pottedTgt) {
                tr.terminal = TerminalCause::Miss;
                ++miss; resolved = true; break;
            }
            ++tr.chainLength;
        }
        if (!resolved) {
            tr.terminal = TerminalCause::ShotCap;
            ++capped;
        }
        if (tr.chainLength >= 0 && tr.chainLength < (int)chainHist.size())
            ++chainHist[tr.chainLength];
        trials.push_back(tr);

        if ((t + 1) % 25 == 0)
            std::fprintf(stderr, "  trial %d/%d (running B&R = %d)\n",
                         t + 1, TRIALS, cleared + golden);
    }

    int brSuccess = cleared + golden;
    const double rate = (double)brSuccess / TRIALS;
    // Wilson 95% CI
    auto wilson = [](int k, int n) -> std::pair<double,double> {
        if (n == 0) return {0,0};
        const double z = 1.96, p = (double)k / n;
        const double denom = 1.0 + z*z/n;
        const double centre = (p + z*z/(2.0*n)) / denom;
        const double half = z * std::sqrt(p*(1-p)/n + z*z/(4.0*n*n))/denom;
        return {std::max(0.0, centre - half), std::min(1.0, centre+half)};
    };
    auto [lo, hi] = wilson(brSuccess, TRIALS);

    std::printf("\n=== break-and-run diagnostic baseline ===\n");
    std::printf("mode: %s\n", ballInHand ? "ball-in-hand (RO-4 sanity)"
                                         : "break + run-out (B&R)");
    if (!ballInHand) {
        std::printf("break: cue=(%.2f, %.2f) aimDz=%.4f speed=%.1f "
                    "a=%.3f b=%.3f (LOCKED)\n",
                    brk.cueX, brk.cueZ, brk.aimDz, brk.speed,
                    brk.a, brk.b);
    }
    std::printf("noise: AIM_SIGMA=%.4f rad, SPEED_SIGMA=%.2f, jitter "
                "<=0.3 mm (break)\n", k::AIM_SIGMA, k::SPEED_SIGMA);
    if (execAimSigma != k::AIM_SIGMA || execSpeedSigma != k::SPEED_SIGMA)
        std::printf("noise: AIM_SIGMA=%.4f rad, SPEED_SIGMA=%.4f "
                    "(run-out OVERRIDE)\n", execAimSigma, execSpeedSigma);
    std::printf("trials: %d, shot-cap: %d, seed-base: %u\n",
                TRIALS, SHOT_CAP, SEED_BASE);
    std::printf("BR-1 (MC scoring): %s%s\n",
                useBr1 ? "ON" : "off",
                useBr1 ? (" (samples=" + std::to_string(br1Samples) +
                          ")").c_str() : "");
    std::printf("BR-2 (rescue shots): %s%s\n\n",
                useBr2 ? "ON" : "off",
                useBr2 ? (" (samples=" + std::to_string(br2Samples) +
                          ", min-pot=" + std::to_string(br2MinPot) +
                          ")").c_str() : "");

    std::printf("=== headline ===\n");
    if (!ballInHand) {
        std::printf("legal-break rate:    %d/%d (%.1f%%)\n",
                    legalBreaks, TRIALS,
                    100.0 * legalBreaks / TRIALS);
        std::printf("mean balls on break: %.2f\n",
                    (double)totalBallsOnBreak / TRIALS);
        std::printf("golden-on-break:     %d/%d (%.1f%%)\n",
                    golden, TRIALS, 100.0 * golden / TRIALS);
    }
    std::printf("BREAK-AND-RUN RATE:  %d/%d (%.1f%%) "
                "95%% CI [%.1f%%, %.1f%%]\n\n",
                brSuccess, TRIALS, 100.0 * rate,
                100.0 * lo, 100.0 * hi);

    std::printf("=== plateau by terminal cause ===\n");
    auto pct = [&](int n){ return 100.0 * n / TRIALS; };
    std::printf("  Cleared        %4d (%.1f%%)  <- legal break + ran out\n",
                cleared, pct(cleared));
    std::printf("  GoldenBreak    %4d (%.1f%%)  <- 9 on legal break\n",
                golden, pct(golden));
    std::printf("  IllegalBreak   %4d (%.1f%%)\n", illegal, pct(illegal));
    std::printf("  Miss           %4d (%.1f%%)\n", miss, pct(miss));
    std::printf("  Scratch        %4d (%.1f%%)\n", scratch, pct(scratch));
    std::printf("  WrongBallFirst %4d (%.1f%%)\n",
                wrongFirst, pct(wrongFirst));
    std::printf("  NoContact      %4d (%.1f%%)\n",
                noContact, pct(noContact));
    std::printf("  SafetyBail     %4d (%.1f%%)\n", bail, pct(bail));
    std::printf("    -- NoLOS         %4d (%.1f%%)  <- positional/break-model "
                "(BR-1 can't help)\n", bailNoLOS, pct(bailNoLOS));
    std::printf("    -- CandsEmpty    %4d (%.1f%%)  <- noiseless seed fails "
                "to pot (BR-1 may help)\n",
                bailCandsEmpty, pct(bailCandsEmpty));
    std::printf("    -- LowValue      %4d (%.1f%%)  <- candidates score "
                "near zero (BR-1's target)\n",
                bailLowValue, pct(bailLowValue));
    std::printf("  ShotCap        %4d (%.1f%%)\n", capped, pct(capped));

    std::printf("\n=== chain-length distribution (post-break shots "
                "potted in order) ===\n");
    for (size_t i = 0; i < chainHist.size(); ++i)
        if (chainHist[i] > 0)
            std::printf("  %zu balls: %d\n", i, chainHist[i]);

    // Per-trial CSV (for downstream analysis)
    if (FILE* f = std::fopen("docs/break_and_run.csv", "w")) {
        std::fprintf(f, "seed,legal_break,nine_on_break,"
                        "balls_potted_on_break,chain_length,terminal\n");
        for (const Trial& tr : trials)
            std::fprintf(f, "%u,%d,%d,%d,%d,%s\n",
                         tr.seed, tr.legalBreak ? 1 : 0,
                         tr.nineOnBreak ? 1 : 0,
                         tr.ballsPottedOnBreak, tr.chainLength,
                         causeName(tr.terminal));
        std::fclose(f);
        std::fprintf(stderr, "wrote docs/break_and_run.csv\n");
    }

    std::printf("\nNOTE: this is a DIAGNOSTIC baseline, NOT the "
                "BR-final gate.\n");
    return 0;
}
