// Trace one break-and-run trial deterministically (given a seed) and
// emit JSON frames for viz/render.py. Mirrors tools/break_and_run.cpp's
// rack-build + break + plan-and-execute loop EXACTLY so the rendered
// trial matches what the harness measures.
//
// Usage:
//   trace_br --seed 7009 --aim-sigma 0.001 --speed-sigma 0.005 \
//            --br1 --br2 --break-cuex 0.55 --break-cuez 1.00 \
//            --break-aim-dz -0.02 --break-speed 8 --break-follow 0 > br.json
//   python viz/render.py br.json br.gif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <sstream>
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

int cueIdx(const std::vector<Ball>& bs) {
    for (size_t i = 0; i < bs.size(); ++i)
        if (bs[i].type == BallType::Cue) return static_cast<int>(i);
    return -1;
}

// EXACT mirror of break_and_run.cpp's rackWithJitter.
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

void emitHeader(std::ostringstream& js, const World& w) {
    js.precision(5);
    js << "{\"R\":" << k::R
       << ",\"table\":{\"xMax\":" << w.table.xMax
       << ",\"zMax\":" << w.table.zMax
       << ",\"pr\":" << w.table.pocketR
       << "},\"frames\":[";
}

// Capture frames from one shot. Returns elapsed simulated time.
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

}  // namespace

int main(int argc, char** argv) {
    unsigned seed = 7009;
    double aimSigma = 0.001, speedSigma = 0.005;
    // Break uses k:: defaults unless overridden, MIRROR break_and_run.cpp.
    double brkAimSigma = k::AIM_SIGMA;
    double brkSpeedSigma = k::SPEED_SIGMA;
    bool useBr1 = true, useBr2 = true;
    int br1Samples = 12, br2Samples = 16;
    double br2MinPot = 0.05;
    double brkCueX = 0.55, brkCueZ = 1.00, brkAimDz = -0.02;
    double brkSpeed = 8.0, brkA = 0.0, brkB = 0.0;
    int planDepth = 2, planBeamK = 3;
    int SHOT_CAP = 12;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--seed" && i + 1 < argc) seed = (unsigned)std::stoul(argv[++i]);
        else if (a == "--aim-sigma" && i + 1 < argc)
            aimSigma = std::stod(argv[++i]);
        else if (a == "--speed-sigma" && i + 1 < argc)
            speedSigma = std::stod(argv[++i]);
        else if (a == "--br1") useBr1 = true;
        else if (a == "--no-br1") useBr1 = false;
        else if (a == "--br1-samples" && i + 1 < argc)
            br1Samples = std::stoi(argv[++i]);
        else if (a == "--br2") useBr2 = true;
        else if (a == "--no-br2") useBr2 = false;
        else if (a == "--br2-samples" && i + 1 < argc)
            br2Samples = std::stoi(argv[++i]);
        else if (a == "--break-cuex" && i + 1 < argc)
            brkCueX = std::stod(argv[++i]);
        else if (a == "--break-cuez" && i + 1 < argc)
            brkCueZ = std::stod(argv[++i]);
        else if (a == "--break-aim-dz" && i + 1 < argc)
            brkAimDz = std::stod(argv[++i]);
        else if (a == "--break-speed" && i + 1 < argc)
            brkSpeed = std::stod(argv[++i]);
        else if (a == "--break-follow" && i + 1 < argc)
            brkB = std::stod(argv[++i]) * k::R;
        else if (a == "--break-aim-sigma" && i + 1 < argc)
            brkAimSigma = std::stod(argv[++i]);
        else if (a == "--break-speed-sigma" && i + 1 < argc)
            brkSpeedSigma = std::stod(argv[++i]);
    }

    difficultyMut().buildOrLoad("difficulty_br.bin", 80);
    setUseMcScoring(useBr1, br1Samples, aimSigma, speedSigma);
    setUseRescueShots(useBr2, br2Samples, br2MinPot);

    std::mt19937 rng(seed);
    World w = rackWithJitter(rng);
    std::ostringstream js;
    bool first = true;
    double tAcc = 0.0;
    emitHeader(js, w);

    // BREAK: mirror break_and_run.cpp's doBreak EXACTLY (same rng order
    // for noise application). Cue is placed, aim built, noise applied,
    // strike, then trace.
    {
        const int ci = cueIdx(w.balls);
        w.balls[ci].r.x = brkCueX;
        w.balls[ci].r.z = brkCueZ;
        Vec3 apex = w.balls[ci].r;
        for (const Ball& b : w.balls) if (b.id == 1) apex = b.r;
        apex.z += brkAimDz;
        Vec3 aim = apex - w.balls[ci].r; aim.y = 0; aim = aim.normalized();
        std::normal_distribution<double> nA(0.0, brkAimSigma);
        std::normal_distribution<double> nS(0.0, brkSpeedSigma);
        const double th = nA(rng), c = std::cos(th), sn = std::sin(th);
        aim = {aim.x * c + aim.z * sn, 0.0, -aim.x * sn + aim.z * c};
        const double v = brkSpeed * (1.0 + nS(rng));
        cueStrike(w.balls[ci], aim, v, brkA, brkB);
        tAcc += emitShot(js, w, tAcc, first);
        std::fprintf(stderr, "break: tEnd=%.3fs\n", tAcc);
    }

    // RUN-OUT: same loop as the harness, same rng consumption.
    std::normal_distribution<double> nA(0.0, aimSigma);
    std::normal_distribution<double> nS(0.0, speedSigma);
    bool resolved = false;
    int shotsTaken = 0;
    for (int s = 0; s < SHOT_CAP; ++s) {
        const int tgt = legalTarget(w.balls);
        if (tgt < 0) { resolved = true; break; }
        RunOutPlan p = planRunOut(w, planDepth, planBeamK);
        if (p.defensive || p.shot.targetId < 0) {
            std::fprintf(stderr, "shot %d: defensive bail (cause=%d)\n",
                         s + 1, (int)p.defCause);
            break;
        }
        const int ci = cueIdx(w.balls);
        const double th = nA(rng), c = std::cos(th), sn = std::sin(th);
        Vec3 aim{p.shot.shot.aim.x * c + p.shot.shot.aim.z * sn, 0.0,
                 -p.shot.shot.aim.x * sn + p.shot.shot.aim.z * c};
        const double v = p.shot.shot.speed * (1.0 + nS(rng));
        cueStrike(w.balls[ci], aim, v, p.shot.shot.a, p.shot.shot.b);
        tAcc += emitShot(js, w, tAcc, first);
        ++shotsTaken;
        std::fprintf(stderr, "shot %d: target=%d pocket=%d tEnd=%.3fs\n",
                     s + 1, p.shot.targetId, p.shot.pocket, tAcc);
    }

    js << "]}";
    std::cout << js.str() << "\n";
    std::fprintf(stderr,
                 "trace_br seed=%u: %d shots, %s (total time %.2fs)\n",
                 seed, shotsTaken, resolved ? "CLEARED" : "did not clear",
                 tAcc);
    return resolved ? 0 : 1;
}
