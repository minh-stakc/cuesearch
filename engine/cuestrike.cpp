#include "engine/cuestrike.h"

#include <algorithm>
#include <cmath>

#include "core/constants.h"

namespace cue {

void cueStrike(Ball& out, const Vec3& aimDir, double speed, double a,
               double b, double mCue) {
    const double R = k::R, m = k::M;
    const double I = k::I_FACTOR * m * R * R;   // (2/5) m R^2

    // Clamp into the miscue circle (Shepard model degenerates beyond R/2).
    const double rmax = 0.5 * R;
    double off = std::sqrt(a * a + b * b);
    if (off > rmax && off > 0.0) {
        const double s = rmax / off;
        a *= s;
        b *= s;
    }

    // Depth along the cue axis to the surface contact point.
    const double c = std::sqrt(std::max(0.0, R * R - a * a - b * b));

    // Orthonormal strike frame: jc = aim, up = +y, ihat = jc x up.
    const Vec3 jc = aimDir.normalized();
    const Vec3 up{0.0, 1.0, 0.0};
    const Vec3 ih = jc.cross(up).normalized();

    // Effective-mass reduction of the linear impulse (cue endmass model).
    const double denom = 1.0 + m / mCue +
                         (5.0 / (2.0 * R * R)) * (a * a + b * b);
    const double F = 2.0 * m * std::fabs(speed) / denom;

    // First-principles spin: contact point P (from centre) x impulse J.
    // (Reference §3's transcribed closed form cancels the vertical-offset
    //  term -> zero follow/draw spin; derive from the cross product instead.)
    const Vec3 P = ih * a + up * b - jc * c;
    const Vec3 J = jc * F;
    out.v = jc * (F / m);
    out.w = P.cross(J) / I;
}

}  // namespace cue
