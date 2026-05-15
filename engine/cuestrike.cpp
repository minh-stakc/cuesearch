// ===== INTERNALIZE =========================================================
// Contact point P (from centre) = a*ihat + b*up - c*jc, c=sqrt(R^2-a^2-b^2).
// Impulse J = F * jc (along the cue). Then v0 = J/m, w0 = (P x J)/I.
//   => pure top/bottom offset b -> spin about the side axis (follow/draw);
//      pure side offset a -> spin about vertical (english). DERIVE, don't
//      recall a closed form (the transcribed reference one cancels at a=0).
// F has the cue endmass denominator. Shepard squirt deflects v0 OPPOSITE the
// english side; |offset| clamped to the miscue circle R/2.
// ===========================================================================
#include "engine/cuestrike.h"

#include <algorithm>
#include <cmath>

#include "core/constants.h"

namespace cue {

namespace {
// Rotate v about unit axis k by angle th (Rodrigues).
Vec3 rotateAbout(const Vec3& v, const Vec3& k, double th) {
    const double c = std::cos(th), s = std::sin(th);
    return v * c + k.cross(v) * s + k * (k.dot(v) * (1.0 - c));
}
}  // namespace

void cueStrike(Ball& out, const Vec3& aimDir, double speed, double a,
               double b, double mCue, double elevationRad, double mbOverMe) {
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
    Vec3 v0 = jc * (F / m);
    Vec3 w0 = P.cross(J) / I;

    // Shepard squirt: side offset deflects the cue ball OPPOSITE the english.
    // tan(alpha) = (5/2)(a/R)sqrt(1-(a/R)^2) / ( mb/me + (5/2)(a/R)^2 )
    if (std::fabs(a) > 1e-12) {
        const double ar = a / R;
        const double alpha =
            std::atan2((2.5) * ar * std::sqrt(std::max(0.0, 1.0 - ar * ar)),
                       mbOverMe + 2.5 * ar * ar);
        v0 = rotateAbout(v0, up, std::copysign(alpha, a));
    }

    // Elevation tilts the spin axis about the side axis -> the sliding
    // parabola then yields a curved (swerve/masse) path. No airborne launch
    // (jump shots are the documented roadmap item).
    if (std::fabs(elevationRad) > 1e-12)
        w0 = rotateAbout(w0, ih, elevationRad);

    out.v = v0;
    out.w = w0;
}

}  // namespace cue
