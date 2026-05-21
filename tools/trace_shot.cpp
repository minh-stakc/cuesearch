// Emit a JSON trace of a shot (or a sequence of shots) for the visualizer.
//   trace_shot follow  > follow.json     single post-collision follow arc
//   trace_shot runout  > runout.json     chain shots until rack is cleared
//   trace_shot break   > break.json      9-ball rack + a real break strike
//   trace_shot -       < layout.txt > shot.json
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
    } else if (mode == "break") {
        // 9-ball break: cue near the head string, hit the apex 1 a hair
        // off-centre with light follow at ~9 m/s -- a firm strong-amateur
        // break that visibly scatters the rack.
        const unsigned seed = argc > 2 ? std::stoul(argv[2]) : 7u;
        World w = breakLayout(seed);
        emitHeader(js, w);
        const int ci = cueIdx(w.balls);
        Vec3 apex = w.balls[ci].r;
        for (const Ball& b : w.balls) if (b.id == 1) apex = b.r;
        apex.z += (seed & 1 ? 1.0 : -1.0) * 0.25 * R;     // slight cut
        Vec3 aim = apex - w.balls[ci].r; aim.y = 0; aim = aim.normalized();
        cueStrike(w.balls[ci], aim, 9.0, 0.0, 0.25 * R);  // light follow
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
