// Emit a random VALID 9-ball layout (B-format) to stdout.
//
//   randtable                 random 1..9 object balls, random seed
//   randtable 5               exactly 5 object balls
//   randtable 5 42            5 balls, fixed seed 42 (reproducible)
//
// The 9 is ALWAYS on the table (the rack only ends when it is potted).
// Balls are non-overlapping, inside the rails, and clear of every pocket.
//
//   randtable | solve -            (pipe a fresh random rack into the solver)
//   randtable 4 7 > t.txt          (save a reproducible 4-ball position)
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <random>
#include <utility>
#include <vector>

#include "core/constants.h"
#include "engine/table.h"

using namespace cue;

int main(int argc, char** argv) {
    const double R = k::R;
    Table tbl;

    int nObj = (argc > 1) ? std::atoi(argv[1]) : 0;          // 0 => random
    unsigned seed = (argc > 2) ? static_cast<unsigned>(std::atoi(argv[2]))
                               : static_cast<unsigned>(std::time(nullptr));
    std::mt19937 rng(seed);

    if (nObj < 1 || nObj > 9)
        nObj = std::uniform_int_distribution<int>(1, 9)(rng);

    // Ball ids: the 9 is always present; fill the rest from {1..8}.
    std::vector<int> ids = {9};
    std::vector<int> pool = {1, 2, 3, 4, 5, 6, 7, 8};
    std::shuffle(pool.begin(), pool.end(), rng);
    for (int i = 0; i < nObj - 1; ++i) ids.push_back(pool[i]);

    std::uniform_real_distribution<double> X(tbl.cxMin() + R, tbl.cxMax() - R);
    std::uniform_real_distribution<double> Z(tbl.czMin() + R, tbl.czMax() - R);
    const auto pockets = tbl.pockets();

    std::vector<std::pair<double, double>> placed;
    auto freeSpot = [&](double& x, double& z) {
        for (int t = 0; t < 400; ++t) {
            x = X(rng);
            z = Z(rng);
            bool ok = true;
            for (auto& p : placed) {
                double dx = p.first - x, dz = p.second - z;
                if (dx * dx + dz * dz < (3.0 * R) * (3.0 * R)) ok = false;
            }
            for (auto& pk : pockets) {
                double dx = pk.x - x, dz = pk.z - z;
                if (dx * dx + dz * dz <
                    (tbl.pocketR + 2.0 * R) * (tbl.pocketR + 2.0 * R))
                    ok = false;
            }
            if (ok) { placed.push_back({x, z}); return true; }
        }
        return false;
    };

    std::printf("# random 9-ball layout (seed %u) — cue + %d object ball(s)\n",
                seed, nObj);
    double x, z;
    if (freeSpot(x, z)) std::printf("B 0 C %.3f %.3f\n", x, z);
    for (int id : ids)
        if (freeSpot(x, z)) std::printf("B %d O %.3f %.3f\n", id, x, z);
    return 0;
}
