// Emit a JSON trace of a shot (or a sequence of shots) for the visualizer.
//   trace_shot follow      > follow.json    single post-collision follow arc
//   trace_shot runout      > runout.json    chain shots until rack is cleared
//   trace_shot break       > break.json     9-ball rack + a real break strike
//   trace_shot best_break  > gold.json      replay the golden_break winner
//                                           (reads docs/golden_best.txt)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "engine/layout.h"
#include "solver/plan.h"

using namespace cue;

namespace {

int cueIdx(const std::vector<Ball>& bs) {
    for (size_t i = 0; i < bs.size(); ++i)
        if (bs[i].type == BallType::Cue) return static_cast<int>(i);
    return -1;
}

void emitHeader(std::ostringstream& js, const World& w) {
    const double R = k::R;
    js.precision(5);
    js << "{\"R\":" << R << ",\"table\":{\"xMax\":" << w.table.xMax
       << ",\"zMax\":" << w.table.zMax << ",\"pr\":" << w.table.pocketR
       << "},\"frames\":[";
}

// Append frames from one shot's trace into `js`, offsetting `t` by
// `tOffset` so the timeline reads continuously across chained shots.
// Returns the elapsed time of this shot (so the caller can accumulate it).
double emitShot(std::ostringstream& js, World& w, double tOffset,
                bool& first) {
    double tEnd = 0.0;
    w.trace(
        [&](double t, const std::vector<Ball>& bs) {
            tEnd = t;
            if (!first) js << ',';
            first = false;
            js << "{\"t\":" << (t + tOffset) << ",\"b\":[";
            bool fb = true;
            for (const Ball& b : bs) {
                if (b.pocketed) continue;
                if (!fb) js << ',';
                fb = false;
                js << "{\"id\":" << b.id << ",\"x\":" << b.r.x
                   << ",\"z\":" << b.r.z << "}";
            }
            js << "]}";
        },
        0.008);
    return tEnd;
}

void emitFooter(std::ostringstream& js) {
    js << "]}";
    std::cout << js.str() << "\n";
}

// 9-ball runout demo layout: 1 near a corner pocket, 9 near another, cue
// mid-table. Pre-checked to be solver-friendly so the chain actually
// finishes -- this is a render demo, not a noise-stressed gate.
World runoutLayout() {
    const double R = k::R;
    World w;
    const auto P = w.table.pockets();
    Ball one;  one.type  = BallType::Object; one.id  = 1;
    one.r = P[0] + Vec3{1, 0, 1}.normalized() * 0.12;
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = P[2] + Vec3{1, 0, -1}.normalized() * 0.12;
    Ball c;    c.type    = BallType::Cue;    c.id    = 0;
    c.r = {1.10, R, 0.55};
    w.balls = {c, one, nine};
    return w;
}

// EXACT mirror of golden_break.cpp's rackWithJitter -- same rng
// consumption order, same slot pattern, cue placed at (0, R, z0) so the
// caller can override X and Z without disturbing the rng sequence. Use
// this (not breakLayout) when reproducing a golden_break search seed.
World rackForSearch(std::mt19937& rng) {
    World w;
    const double R = k::R;
    const double s     = 2.0 * R * 1.015;
    const double pitch = s * 0.86602540378;
    const double fx = 0.75 * w.table.xMax;
    const double z0 = 0.5  * w.table.zMax;
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
    int id[9];  id[0] = 1;  id[4] = 9;
    for (int k = 0, r = 0; k < 9; ++k)
        if (k != 0 && k != 4) id[k] = rest[r++];

    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = {0.0, R, z0};
    w.balls.push_back(cue);
    for (int k = 0; k < 9; ++k) {
        Ball b;
        b.type = BallType::Object;
        b.id   = id[k];
        b.r    = {slot[k].x + J(rng), R, slot[k].z + J(rng)};
        w.balls.push_back(b);
    }
    return w;
}

// A tight 9-ball diamond on the foot spot: 1 at the apex toward the
// breaker, 9 dead centre, the rest shuffled. Cue in the kitchen on the
// head-string side -- mirrors tools/match.cpp's break setup so the
// render matches what the solver actually plays.
World breakLayout(unsigned seed) {
    World w;
    const double R = k::R;
    const double s  = 2.0 * R * 1.015;          // ~0.9 mm contact gap
    const double pitch = s * 0.86602540378;     // sqrt(3)/2 row pitch
    const double fx = 0.75 * w.table.xMax;      // foot spot x
    const double z0 = 0.5  * w.table.zMax;      // foot spot z
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> J(-3e-4, 3e-4);

    struct Slot { double x, z; };
    Slot slot[9] = {
        {fx,             z0},                   // 0 apex (1)
        {fx + pitch,     z0 - s / 2},
        {fx + pitch,     z0 + s / 2},
        {fx + 2 * pitch, z0 - s},
        {fx + 2 * pitch, z0},                   // 4 centre (9)
        {fx + 2 * pitch, z0 + s},
        {fx + 3 * pitch, z0 - s / 2},
        {fx + 3 * pitch, z0 + s / 2},
        {fx + 4 * pitch, z0},                   // 8 back
    };
    std::vector<int> rest = {2, 3, 4, 5, 6, 7, 8};
    std::shuffle(rest.begin(), rest.end(), rng);
    int id[9];  id[0] = 1; id[4] = 9;
    for (int k = 0, r = 0; k < 9; ++k)
        if (k != 0 && k != 4) id[k] = rest[r++];

    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = {0.20 * w.table.xMax, R, z0 + J(rng) * 8.0};   // kitchen
    w.balls.push_back(cue);
    for (int k = 0; k < 9; ++k) {
        Ball b;
        b.type = BallType::Object;
        b.id   = id[k];
        b.r    = {slot[k].x + J(rng), R, slot[k].z + J(rng)};
        w.balls.push_back(b);
    }
    return w;
}

}  // namespace

int main(int argc, char** argv) {
    const double R = k::R;
    const std::string mode = argc > 1 ? argv[1] : "follow";
    std::ostringstream js;
    bool first = true;
    double tAcc = 0.0;

    if (mode == "runout") {
        // Chain shots: plan -> strike -> trace, until the rack is
        // cleared or a shot fails. Each call to emitShot advances the
        // world to rest and appends its frames to the running timeline.
        World w = runoutLayout();
        emitHeader(js, w);
        const int CAP = 6;
        for (int s = 0; s < CAP; ++s) {
            const int tgt = legalTarget(w.balls);
            if (tgt < 0) break;                       // rack cleared
            PlanResult p = planRunout(w, 2, 24, 4, 2, 3);
            if (p.shot.targetId < 0) break;
            const int ci = cueIdx(w.balls);
            cueStrike(w.balls[ci], p.shot.shot.aim, p.shot.shot.speed,
                      p.shot.shot.a, p.shot.shot.b);
            tAcc += emitShot(js, w, tAcc, first);
            // Stop if the cue scratched OR the legal target survived
            // (a missed shot -- nothing left to chain into).
            if (w.balls[ci].pocketed) break;
            bool pottedTgt = true;
            for (const Ball& b : w.balls)
                if (b.id == tgt && !b.pocketed) { pottedTgt = false; break; }
            if (!pottedTgt) break;
        }
        emitFooter(js);
    } else if (mode == "break" || mode == "best_break") {
        // 9-ball break. Default ("break"): a firm strong-amateur break,
        // apex hit a hair off-centre with light follow at ~9 m/s.
        // best_break: replay the winning parameters from golden_break,
        // read from docs/golden_best.txt.
        const unsigned seed = argc > 2 ? std::stoul(argv[2]) : 7u;
        double cueX = -1.0, cueZ = -1.0;
        double aimDz = (seed & 1 ? 1.0 : -1.0) * 0.25 * R;
        double speed = 9.0, tipA = 0.0, tipB = 0.25 * R;
        bool addNoise = false;
        if (mode == "best_break") {
            std::ifstream in("docs/golden_best.txt");
            if (!in) {
                std::fprintf(stderr,
                    "best_break: docs/golden_best.txt not found -- run "
                    "./build/golden_break first\n");
                return 2;
            }
            // Parse line-by-line; skip comments and blanks. (The
            // previous `in >> key >> val` form choked on the `#` header
            // line and silently fell back to default cue placement.)
            std::string line;
            while (std::getline(in, line)) {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream iss(line);
                std::string key; double val;
                if (!(iss >> key >> val)) continue;
                if (key == "cueX")       cueX  = val;
                else if (key == "cueZ")  cueZ  = val;
                else if (key == "aimDz") aimDz = val;
                else if (key == "speed") speed = val;
                else if (key == "a")     tipA  = val;
                else if (key == "b")     tipB  = val;
            }
            std::fprintf(stderr,
                "best_break: parsed cueX=%.4f cueZ=%.4f aimDz=%.5f "
                "speed=%.2f a=%.5f b=%.5f\n",
                cueX, cueZ, aimDz, speed, tipA, tipB);
            addNoise = true;
        }
        // Helper: build the break + apply strike at seed s. Mirrors
        // golden_break.cpp's oneBreak EXACTLY when addNoise is true so
        // the reproducer rate matches the search. Returns the world
        // ready for simulateShot or w.trace.
        auto buildBreak = [&](unsigned s) -> World {
            World ww;
            int ci2;
            if (addNoise) {
                std::mt19937 rng(s);
                ww = rackForSearch(rng);   // SAME rng pattern as the search
                ci2 = cueIdx(ww.balls);
                if (cueX > 0.0) ww.balls[ci2].r.x = cueX;
                if (cueZ > 0.0) ww.balls[ci2].r.z = cueZ;
                Vec3 ap = ww.balls[ci2].r;
                for (const Ball& b : ww.balls) if (b.id == 1) ap = b.r;
                ap.z += aimDz;
                Vec3 am = ap - ww.balls[ci2].r; am.y = 0;
                am = am.normalized();
                std::normal_distribution<double> nA(0.0, k::AIM_SIGMA);
                std::normal_distribution<double> nS(0.0, k::SPEED_SIGMA);
                const double th = nA(rng), c = std::cos(th), sn = std::sin(th);
                am = {am.x * c + am.z * sn, 0.0, -am.x * sn + am.z * c};
                const double v = speed * (1.0 + nS(rng));
                cueStrike(ww.balls[ci2], am, v, tipA, tipB);
            } else {
                ww = breakLayout(s);
                ci2 = cueIdx(ww.balls);
                Vec3 ap = ww.balls[ci2].r;
                for (const Ball& b : ww.balls) if (b.id == 1) ap = b.r;
                ap.z += aimDz;
                Vec3 am = ap - ww.balls[ci2].r; am.y = 0;
                am = am.normalized();
                cueStrike(ww.balls[ci2], am, speed, tipA, tipB);
            }
            return ww;
        };

        unsigned usedSeed = seed;
        if (mode == "best_break") {
            // VERIFY the reproducer matches the search before picking a
            // cherry-picked golden seed. The rack-build + noise rng now
            // mirrors golden_break::oneBreak exactly, so the measured
            // rate here should be within the search's 95% CI [2%, 7.7%].
            // If not, fail loudly rather than render a misleading gif.
            const int VERIFY_N = 1000;
            int goldens = 0, firstGolden = -1;
            for (int s = 1; s <= VERIFY_N; ++s) {
                World ww = buildBreak((unsigned)s);
                ShotOutcome o = simulateShot(ww);
                if (o.won && o.foul == Foul::None) {
                    ++goldens;
                    if (firstGolden < 0) firstGolden = s;
                }
            }
            const double rate = (double)goldens / VERIFY_N;
            std::fprintf(stderr,
                "best_break: reproducer verification = %d/%d goldens = "
                "%.2f%%\n", goldens, VERIFY_N, 100.0 * rate);
            if (firstGolden < 0) {
                std::fprintf(stderr,
                    "best_break: ZERO goldens at the optimal params in "
                    "%d seeds -- reproducer mismatch; refusing to render "
                    "a misleading gif.\n", VERIFY_N);
                return 3;
            }
            usedSeed = (unsigned)firstGolden;
            std::fprintf(stderr,
                "best_break: rendering first-golden seed = %u\n",
                usedSeed);
            // Append the verified rate to golden_best.txt so the plotter
            // can show the unbiased number alongside the search's biased
            // point estimate (selection-bias correction).
            if (std::FILE* f = std::fopen("docs/golden_best.txt", "a")) {
                std::fprintf(f, "pGoldVerified %.5f\n", rate);
                std::fprintf(f, "verifySeeds %d\n", VERIFY_N);
                std::fprintf(f, "verifyGoldens %d\n", goldens);
                std::fprintf(f, "renderSeed %u\n", usedSeed);
                std::fclose(f);
            }
        }
        World w = buildBreak(usedSeed);
        emitHeader(js, w);
        tAcc += emitShot(js, w, tAcc, first);
        emitFooter(js);
    } else {
        // "follow": cue cuts a ball with topspin -> post-collision arc.
        World w;
        Ball b; b.type = BallType::Object; b.id = 1; b.r = {1.4, R, 0.6};
        Ball c; c.type = BallType::Cue;    c.id = 0; c.r = {0.5, R, 0.45};
        w.balls = {c, b};
        emitHeader(js, w);
        cueStrike(w.balls[0], Vec3{1, 0, 0.12}.normalized(), 3.0, 0.0,
                  0.35 * R);
        tAcc += emitShot(js, w, tAcc, first);
        emitFooter(js);
    }
    return 0;
}
