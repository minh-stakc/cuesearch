// One-shot diagnostic: replay the optimal break (golden_best.txt params,
// seed=186) and dump every simulator event in a target time window.
// Use to see whether ball-ball events around suspect overlap times are
// (a) scheduled and applied, (b) scheduled but somehow skipped, or
// (c) never scheduled.
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "engine/world.h"

using namespace cue;

namespace {

World rackForSearch(std::mt19937& rng) {
    World w;
    const double R = k::R;
    const double s = 2.0 * R * 1.015, pitch = s * 0.86602540378;
    const double fx = 0.75 * w.table.xMax, z0 = 0.5 * w.table.zMax;
    std::uniform_real_distribution<double> J(-3e-4, 3e-4);
    struct Slot { double x, z; };
    Slot slot[9] = {
        {fx, z0}, {fx + pitch, z0 - s/2}, {fx + pitch, z0 + s/2},
        {fx + 2*pitch, z0 - s}, {fx + 2*pitch, z0}, {fx + 2*pitch, z0 + s},
        {fx + 3*pitch, z0 - s/2}, {fx + 3*pitch, z0 + s/2},
        {fx + 4*pitch, z0},
    };
    std::vector<int> rest = {2, 3, 4, 5, 6, 7, 8};
    std::shuffle(rest.begin(), rest.end(), rng);
    int id[9]; id[0] = 1; id[4] = 9;
    for (int kk=0,r=0; kk<9; ++kk) if (kk!=0 && kk!=4) id[kk] = rest[r++];
    Ball cue; cue.type = BallType::Cue; cue.id = 0;
    cue.r = {0.0, R, z0}; w.balls.push_back(cue);
    for (int kk=0; kk<9; ++kk) {
        Ball b; b.type = BallType::Object; b.id = id[kk];
        b.r = {slot[kk].x + J(rng), R, slot[kk].z + J(rng)};
        w.balls.push_back(b);
    }
    return w;
}

const char* evname(EventType t) {
    switch (t) {
        case EventType::PhaseEnd: return "PhaseEnd";
        case EventType::Pocket:   return "Pocket";
        case EventType::Cushion:  return "Cushion";
        case EventType::BallBall: return "BallBall";
    }
    return "?";
}

}  // namespace

int main(int argc, char** argv) {
    const double tlo = argc > 1 ? std::atof(argv[1]) : 2.2;
    const double thi = argc > 2 ? std::atof(argv[2]) : 2.5;
    const unsigned seed = argc > 3 ? (unsigned)std::atoi(argv[3]) : 186u;

    double cueX = 0.15, cueZ = 0.90, aimDz = -0.00857;
    double speed = 11.0, tipA = 0.0, tipB = -0.00857;
    {
        std::ifstream in("docs/golden_best.txt");
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            std::string key; double val;
            if (!(iss >> key >> val)) continue;
            if (key == "cueX") cueX = val;
            else if (key == "cueZ") cueZ = val;
            else if (key == "aimDz") aimDz = val;
            else if (key == "speed") speed = val;
            else if (key == "a") tipA = val;
            else if (key == "b") tipB = val;
        }
    }

    std::mt19937 rng(seed);
    World w = rackForSearch(rng);
    int ci = -1;
    for (size_t i=0; i<w.balls.size(); ++i)
        if (w.balls[i].type == BallType::Cue) { ci = (int)i; break; }
    w.balls[ci].r.x = cueX;
    w.balls[ci].r.z = cueZ;
    Vec3 apex = w.balls[ci].r;
    for (const Ball& b : w.balls) if (b.id == 1) apex = b.r;
    apex.z += aimDz;
    Vec3 aim = apex - w.balls[ci].r; aim.y = 0; aim = aim.normalized();
    std::normal_distribution<double> nA(0.0, k::AIM_SIGMA);
    std::normal_distribution<double> nS(0.0, k::SPEED_SIGMA);
    const double th = nA(rng), c = std::cos(th), sn = std::sin(th);
    aim = {aim.x * c + aim.z * sn, 0.0, -aim.x * sn + aim.z * c};
    const double v = speed * (1.0 + nS(rng));
    cueStrike(w.balls[ci], aim, v, tipA, tipB);

    std::printf("# events in t in [%.3f, %.3f] for seed=%u\n", tlo, thi, seed);
    const double R = k::R;
    int idx = 0;
    w.simulate([&](double t, const WorldEvent& e,
                   const std::vector<Ball>& bs) {
        if (t < tlo || t > thi) { ++idx; return; }
        std::printf("[%4d] t=%.4f %-8s i=%d j=%d rail=%d\n",
                    idx++, t, evname(e.type), e.i, e.j, e.rail);
        // Flag any ball whose y has drifted from the slate (R).
        for (const Ball& b : bs) {
            if (b.pocketed) continue;
            const double dy = b.r.y - R;
            if (std::fabs(dy) > 1e-4) {
                std::printf("        AIRBORNE: id=%d y=%.5f (dy=%+.5f m, "
                            "vy=%+.4f m/s)\n",
                            b.id, b.r.y, dy, b.v.y);
            }
        }
        // Also: show distances between every active ball pair under 1.2 diam
        const double R = k::R, diam = 2.0 * R;
        for (size_t i=0; i<bs.size(); ++i)
            for (size_t j=i+1; j<bs.size(); ++j) {
                if (bs[i].pocketed || bs[j].pocketed) continue;
                const double dx = bs[i].r.x - bs[j].r.x;
                const double dz = bs[i].r.z - bs[j].r.z;
                const double d = std::sqrt(dx*dx + dz*dz);
                if (d < 1.2 * diam) {
                    std::printf("        pair ids %d,%d dist=%.4f diam\n",
                                bs[i].id, bs[j].id, d/diam);
                }
            }
    });
    return 0;
}
