// Golden-break parameter search.
//
// What:  search the (cue X, cue Z, aim cut on the 1, cue speed, side
//        spin, follow/draw) grid for the parameters that maximise
//        P(9 pocketed on a LEGAL break) -- the "golden break" / instant
//        win in 9-ball.
//
// Why:   the project's thesis is search/optimisation over a stochastic
//        forward model. The break is a high-energy, high-variance shot
//        where small parameter changes matter -- a natural showcase.
//
// Rule:  WPA 9-ball break -- the cue ball may be placed at ANY point
//        behind the head string (the 2nd diamond from the shooter's
//        short rail). On a regulation 9-foot table (xMax = 2.54 m) the
//        head string is at x = 0.635 m, so legal cueX is [0, 0.635].
//        The search respects this (cueX swept inside the kitchen).
//
// How:   coarse 6-D grid, N noisy rollouts per cell (rack positional
//        jitter + calibrated aim/speed noise). Sort by P(gold). Two-
//        stage tournament: 4x samples on the top-10 to limit stage-1
//        selection bias. Print Wilson 95% CI on the displayed number.
//        Dump the full grid as CSV (docs/golden_grid.csv) so the Python
//        plotter can render heatmaps and the table cartoon. Save the
//        winner so trace_shot best_break can render it as a gif.
//
// Honesty (the established discipline):
//  * Calibrated noise (k::AIM_SIGMA / SPEED_SIGMA) and rack jitter ARE
//    applied. We are NOT searching the perfect-execution space.
//  * "Legal break" here = "no foul" (cue hit the 1, no scratch). The
//    exact WPA 4-balls-to-a-rail clause needs rail-contact tracking; at
//    8-15 m/s the simplification is approximately tight and stated.
//  * The reported P(gold) is conditional on this engine's break model.
//    The break model is NOT validated against tournament stats yet
//    (that is BR-2 in docs/BREAK_AND_RUN.md). Observed pro-tournament
//    golden-break rates are single-digit percent; far above that band,
//    suspect the break model, not the optimiser.
//  * Stage-2 Wilson CI is honest about stage-2 sampling, but a faint
//    upward selection bias from picking the stage-1 max remains. For a
//    bias-corrected number at the winning cell, run `trace_shot
//    best_break` -- it does a 1000-seed reproducer check, writes the
//    verified rate back to docs/golden_best.txt, and refuses to render
//    a misleading gif if the reproducer rate is zero.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "engine/world.h"

using namespace cue;

namespace {

struct Params {
    double cueX;       // cue X (m, in the kitchen [0, 0.25*xMax])
    double cueZ;       // cue Z (m, across table width)
    double aimDz;      // aim offset from apex centre in z (m, signed = cut)
    double speed;      // cue speed (m/s)
    double a;          // side tip offset (m, right +)
    double b;          // vertical tip offset (m, up = follow)
};

struct Cell {
    Params p;
    int trials = 0;
    int legal  = 0;
    int gold   = 0;
    double pLeg()  const { return trials ? (double)legal / trials : 0.0; }
    double pGold() const { return trials ? (double)gold  / trials : 0.0; }
};

World rackWithJitter(std::mt19937& rng) {
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
    cue.r = {0.0, R, z0};   // X / Z set by caller
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

int cueIdx(const World& w) {
    for (size_t i = 0; i < w.balls.size(); ++i)
        if (w.balls[i].type == BallType::Cue) return static_cast<int>(i);
    return -1;
}

std::pair<bool, bool> oneBreak(const Params& p, unsigned seed) {
    std::mt19937 rng(seed);
    World w = rackWithJitter(rng);
    const int ci = cueIdx(w);
    w.balls[ci].r.x = p.cueX;
    w.balls[ci].r.z = p.cueZ;

    Vec3 apex = w.balls[ci].r;
    for (const Ball& b : w.balls) if (b.id == 1) apex = b.r;
    apex.z += p.aimDz;
    Vec3 aim = apex - w.balls[ci].r; aim.y = 0; aim = aim.normalized();

    std::normal_distribution<double> nA(0.0, k::AIM_SIGMA);
    std::normal_distribution<double> nS(0.0, k::SPEED_SIGMA);
    const double th = nA(rng), c = std::cos(th), sn = std::sin(th);
    const Vec3 aimN{aim.x * c + aim.z * sn, 0.0, -aim.x * sn + aim.z * c};
    const double v = p.speed * (1.0 + nS(rng));

    cueStrike(w.balls[ci], aimN, v, p.a, p.b);
    ShotOutcome o = simulateShot(w);
    const bool legal = (o.foul == Foul::None);
    const bool gold  = o.won && legal;
    return {legal, gold};
}

Cell scoreCell(const Params& p, int samples, unsigned baseSeed) {
    Cell c; c.p = p;
    for (int i = 0; i < samples; ++i) {
        auto [leg, gld] =
            oneBreak(p, baseSeed * 2654435761u + static_cast<unsigned>(i));
        c.trials += 1;
        if (leg) c.legal += 1;
        if (gld) c.gold  += 1;
    }
    return c;
}

std::pair<double, double> wilson95(int k, int n) {
    if (n == 0) return {0.0, 0.0};
    const double z = 1.96, p = static_cast<double>(k) / n;
    const double denom = 1.0 + z * z / n;
    const double centre = (p + z * z / (2.0 * n)) / denom;
    const double half =
        z * std::sqrt(p * (1 - p) / n + z * z / (4.0 * n * n)) / denom;
    return {std::max(0.0, centre - half), std::min(1.0, centre + half)};
}

}  // namespace

int main(int argc, char** argv) {
    int samples = 40;
    bool full = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--samples" && i + 1 < argc) samples = std::stoi(argv[++i]);
        else if (a == "--full")  full = true;
        else if (a == "--quick") full = false;
    }
    const double R = k::R;

    World probe;
    const double xMax = probe.table.xMax;
    const double zMax = probe.table.zMax;
    const double headStringX = 0.25 * xMax;             // = 0.635 m

    // Cue X: anywhere behind the head string. Avoid the very head rail
    // (cue ball too close to the back limits stroke arc -- not how
    // anyone breaks). cueZ: across the table width, full sweep.
    std::vector<double> cueXs = full
        ? std::vector<double>{0.10, 0.20, 0.30, 0.40, 0.55}
        : std::vector<double>{0.15, 0.30, 0.50};
    std::vector<double> cueZs = full
        ? std::vector<double>{0.20, 0.35, 0.50, 0.65, 0.80, 0.95, 1.10}
        : std::vector<double>{0.30, 0.50, 0.70, 0.90, 1.10};
    std::vector<double> aimDz = full
        ? std::vector<double>{-0.9 * R, -0.6 * R, -0.3 * R, 0.0,
                              0.3 * R, 0.6 * R, 0.9 * R}
        : std::vector<double>{-0.6 * R, -0.3 * R, 0.0, 0.3 * R, 0.6 * R};
    std::vector<double> speeds = full
        ? std::vector<double>{6.0, 8.0, 10.0, 12.0, 14.0, 16.0}
        : std::vector<double>{8.0, 11.0, 14.0};
    std::vector<double> as = {-0.3 * R, 0.0, 0.3 * R};
    std::vector<double> bs = {-0.3 * R, 0.0, 0.3 * R};

    std::vector<Params> grid;
    for (double cx : cueXs)
        for (double cz : cueZs)
            for (double az : aimDz)
                for (double sp : speeds)
                    for (double aa : as)
                        for (double bb : bs)
                            grid.push_back({cx, cz, az, sp, aa, bb});

    std::fprintf(stderr,
        "golden_break: grid=%zu cells, samples=%d/cell -> %zu breaks "
        "(stage 1)\n", grid.size(), samples, grid.size() * (size_t)samples);
    std::fprintf(stderr,
        "  legal cue placement: x in [0, %.3f] m (head string), "
        "z in [0, %.3f] m\n", headStringX, zMax);

    std::vector<Cell> cells;
    cells.reserve(grid.size());
    for (size_t i = 0; i < grid.size(); ++i) {
        cells.push_back(scoreCell(grid[i], samples, 1u + (unsigned)i));
        if ((i + 1) % 50 == 0 || i + 1 == grid.size())
            std::fprintf(stderr, "  stage 1: %zu/%zu cells\n",
                         i + 1, grid.size());
    }

    // Dump the FULL grid for the plotter (heatmaps, marginals, etc.).
    if (FILE* f = std::fopen("docs/golden_grid.csv", "w")) {
        std::fprintf(f, "cueX,cueZ,aimDz,speed,a,b,trials,legal,gold\n");
        for (const Cell& c : cells) {
            std::fprintf(f, "%.5f,%.5f,%.5f,%.3f,%.5f,%.5f,%d,%d,%d\n",
                         c.p.cueX, c.p.cueZ, c.p.aimDz, c.p.speed,
                         c.p.a, c.p.b, c.trials, c.legal, c.gold);
        }
        std::fclose(f);
        std::fprintf(stderr, "wrote docs/golden_grid.csv (%zu rows)\n",
                     cells.size());
    }

    std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
        if (a.gold != b.gold) return a.gold > b.gold;
        return a.legal > b.legal;
    });

    const int TOP = (int)std::min<size_t>(10, cells.size());
    std::vector<Cell> refined;
    refined.reserve(TOP);
    for (int i = 0; i < TOP; ++i) {
        refined.push_back(
            scoreCell(cells[i].p, samples * 4, 100000u + (unsigned)i));
    }
    std::sort(refined.begin(), refined.end(),
              [](const Cell& a, const Cell& b) {
                  return a.pGold() > b.pGold();
              });

    std::printf("\n=== golden_break sweep ===\n");
    std::printf("WPA rule: cue ball anywhere behind the head string "
                "(2nd diamond from the\n  shooter's short rail). On a "
                "regulation 9-ft table (xMax=%.2f m) that\n  is x in "
                "[0, %.3f m]; both dimensions swept inside the legal "
                "kitchen.\n\n", xMax, headStringX);
    std::printf("grid=%zu cells, stage-1 %d samples/cell, "
                "stage-2 %d samples on top-%d\n",
                grid.size(), samples, samples * 4, TOP);
    std::printf("noise: AIM_SIGMA=%.4f rad, SPEED_SIGMA=%.2f, "
                "rack jitter <=0.3 mm\n", k::AIM_SIGMA, k::SPEED_SIGMA);
    std::printf("legal-break definition: no foul (cue hit the 1, no "
                "scratch). The WPA\n  4-balls-to-a-rail clause is "
                "approximated; at the speeds searched any\n  non-pot "
                "non-scratch shot drives the rack to the rails -- "
                "tight.\n");
    std::printf("conditional: P(gold) is *given* this engine's break "
                "model.\n  Reference: golden break is rare in pro play "
                "(single-digit percent;\n  a sanity-check, not a "
                "citation). Far above that band -> the break\n  model "
                "is the suspect, not the optimiser. NB: the displayed\n  "
                "stage-2 P(gold) is the best-in-grid -- run "
                "trace_shot best_break for a\n  1000-seed bias-corrected "
                "re-evaluation at the winning cell.\n");
    std::printf("selection: displayed P(gold) is best-in-grid; faint "
                "upward bias from\n  picking the stage-1 max remains "
                "even after the 4x re-eval.\n\n");

    std::printf("%-6s %-6s %-8s %-6s %-7s %-7s | %-8s %-7s %s\n",
                "cueX", "cueZ", "aimDz/R", "speed", "a/R", "b/R",
                "P(legal)", "P(gold)", "P(gold) 95% CI");
    std::printf("--------------------------------------------------+"
                "----------------------------------\n");
    for (const Cell& c : refined) {
        auto [lo, hi] = wilson95(c.gold, c.trials);
        std::printf("%-6.2f %-6.2f %+8.2f %-6.1f %+7.2f %+7.2f | "
                    "%-8.2f %-7.3f [%.3f, %.3f]\n",
                    c.p.cueX, c.p.cueZ, c.p.aimDz / R, c.p.speed,
                    c.p.a / R, c.p.b / R,
                    c.pLeg(), c.pGold(), lo, hi);
    }

    const Cell& best = refined[0];
    if (FILE* f = std::fopen("docs/golden_best.txt", "w")) {
        std::fprintf(f, "# golden_break winner -- consumed by trace_shot best_break\n");
        std::fprintf(f, "cueX  %.5f\n", best.p.cueX);
        std::fprintf(f, "cueZ  %.5f\n", best.p.cueZ);
        std::fprintf(f, "aimDz %.5f\n", best.p.aimDz);
        std::fprintf(f, "speed %.3f\n", best.p.speed);
        std::fprintf(f, "a     %.5f\n", best.p.a);
        std::fprintf(f, "b     %.5f\n", best.p.b);
        std::fprintf(f, "pGold %.4f\n", best.pGold());
        std::fclose(f);
        std::fprintf(stderr, "wrote docs/golden_best.txt\n");
    }
    return 0;
}
