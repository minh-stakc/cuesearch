#pragma once
// RO-1: precomputed pot-probability difficulty table (PickPocket / CueCard
// canonical structure). P(pot) as a function of geometric features, filled
// once from noisy sims (k::AIM_SIGMA) and cached. Replaces per-node live
// Monte-Carlo -> the run-out search becomes fast AND consistent (the live
// MC is exactly why POS-b's depth-2 search timed out / was noisy).
//
// Features (3-D; PickPocket used 4 incl. an OB->pocket-facing angle, which
// our circular-pocket model largely subsumes -- documented simplification):
//   alpha : cut angle, deg   [0 .. 85]
//   dCO   : cue -> object distance, m
//   dOP   : object -> pocket distance, m
#include <string>
#include <vector>

namespace cue {

struct DifficultyTable {
    // P(legal-geometry pot of the object) for the given shot features.
    // Trilinear-interpolated, clamped at the grid edges.
    double potProb(double alphaDeg, double dCO, double dOP) const;

    // Build from noisy sims on CP7-validated geometry (slow: ~grid*sims
    // simulateShot calls). Cached to `cacheFile`; reloaded if present.
    void buildOrLoad(const std::string& cacheFile, int simsPerCell = 80);

    std::vector<double> grid;     // NA*ND*ND, row-major (alpha, dCO, dOP)
    bool ready = false;
};

// Process-wide table. Call difficultyMut().buildOrLoad(...) once at
// startup; mobilityValue / the run-out search read difficulty().
const DifficultyTable& difficulty();
DifficultyTable& difficultyMut();

}  // namespace cue
