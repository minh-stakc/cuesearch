// Throughput benchmark: physics sims/sec and solver rollouts/sec. Numbers
// here go on the resume ("N shots/sec, M rollouts/sec").
#include <chrono>
#include <cstdio>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "solver/solver.h"

using namespace cue;
using clk = std::chrono::steady_clock;

int main() {
    World w;
    const Vec3 P = w.table.pockets()[0];
    const Vec3 out = Vec3{1, 0, 1}.normalized();
    Ball t; t.type = BallType::Object; t.id = 1; t.r = P + out * 0.28;
    Ball c; c.type = BallType::Cue; c.id = 0; c.r = t.r + out * 0.30;
    w.balls = {c, t};

    // Raw physics: full shot simulations / second.
    {
        const int N = 20000;
        auto t0 = clk::now();
        for (int i = 0; i < N; ++i) {
            World ww = w;
            cueStrike(ww.balls[0], out * -1.0, 2.5, 0.0, 0.0);
            ww.simulate();
        }
        double s = std::chrono::duration<double>(clk::now() - t0).count();
        std::printf("physics:  %d shots in %.3fs  =>  %.0f shots/sec\n", N, s,
                    N / s);
    }

    // Solver: Monte-Carlo rollouts / second (parallel).
    {
        auto cands = candidateShots(w);
        const int ROLL = 4000;
        auto t0 = clk::now();
        volatile double sink = 0.0;
        for (int rep = 0; rep < 8; ++rep)
            sink += evaluate(w, cands.front(), ROLL, 1u + rep);
        double s = std::chrono::duration<double>(clk::now() - t0).count();
        (void)sink;
        std::printf("solver:   %d rollouts in %.3fs  =>  %.0f rollouts/sec\n",
                    ROLL * 8, s, ROLL * 8 / s);
    }
    return 0;
}
