#pragma once
// 9-ft pool surface. CP5: 4 continuous axis-aligned cushions (Mathavan
// spin-coupled rebound) + 6 circular pocket capture zones. Finite-segment
// jaw rattle is a documented roadmap item (not modelled).
#include <array>

#include "core/constants.h"
#include "core/vec3.h"

namespace cue {

struct Table {
    // Playing surface extents (m): regulation 9-ft = 100" x 50".
    double xMin = 0.0,           xMax = 2.54;
    double zMin = 0.0,           zMax = 1.27;

    // Pocket capture radius (corner/side mouths ~ 2.25x ball dia).
    double pocketR = 0.0635;

    double cxMin() const { return xMin + k::R; }
    double cxMax() const { return xMax - k::R; }
    double czMin() const { return zMin + k::R; }
    double czMax() const { return zMax - k::R; }

    // 6 pocket centres: 4 corners + 2 side (mid long rails).
    std::array<Vec3, 6> pockets() const {
        const double mx = 0.5 * (xMin + xMax);
        return {Vec3{xMin, k::R, zMin}, Vec3{xMax, k::R, zMin},
                Vec3{xMin, k::R, zMax}, Vec3{xMax, k::R, zMax},
                Vec3{mx, k::R, zMin},   Vec3{mx, k::R, zMax}};
    }
};

}  // namespace cue
