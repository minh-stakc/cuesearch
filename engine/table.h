#pragma once
// 9-ft pool playing surface. Pockets/jaws are CP5 — CP2 rails are 4
// axis-aligned cushions (placeholder reflection physics, see world.cpp).
#include "core/constants.h"

namespace cue {

struct Table {
    // Playing surface extents (m): regulation 9-ft = 100" x 50".
    double xMin = 0.0,           xMax = 2.54;
    double zMin = 0.0,           zMax = 1.27;

    // Ball-center bounds (inset by R).
    double cxMin() const { return xMin + k::R; }
    double cxMax() const { return xMax - k::R; }
    double czMin() const { return zMin + k::R; }
    double czMax() const { return zMax - k::R; }
};

}  // namespace cue
