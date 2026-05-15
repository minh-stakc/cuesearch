// ===== INTERNALIZE =========================================================
// Contact raised to h = 7R/5 => contact normal nC tilts up by th,
// sin th = 2/5. Same impulse-integration pattern as ball-ball: total normal
// impulse (1+e) m vN along nC, Coulomb friction over it. Sidespin enters the
// contact slip via w x (R nC) -> running english reduces slip (longer
// rebound), reverse increases it (shorter). e is the empirical pool COR
// (fit so perpendicular retention ~ Dr. Dave ~0.5-0.6). Vertical impulse
// component is absorbed by the slate (non-jump regime).
// ===========================================================================
#include "engine/resolve_cushion.h"

#include <cmath>

#include "core/constants.h"

namespace cue {

void resolveCushion(Ball& b, const Vec3& outwardNormal) {
    const double m = k::M, R = k::R;
    const double I = k::I_FACTOR * m * R * R;
    const double e = k::E_CUSHION_POOL, muw = k::MU_CUSHION;

    // Contact raised by h-R: contact normal tilts up by th, sin th = 2/5.
    const double sinT = 2.0 / 5.0;
    const double cosT = std::sqrt(1.0 - sinT * sinT);   // sqrt(21)/5
    const Vec3 up{0.0, 1.0, 0.0};
    const Vec3 o = outwardNormal.normalized();
    const Vec3 nC = (o * cosT + up * sinT).normalized();  // centre->contact
    const Vec3 rC = nC * R;                                // contact offset

    const double vN = b.v.dot(nC);
    if (vN <= 0.0) return;                                  // not approaching

    // Decoupled normal restitution fixes total normal impulse (CP3 pattern).
    const double Pf = (1.0 + e) * m * vN;
    const int N = 2000;
    const double dP = Pf / N;

    for (int s = 0; s < N; ++s) {
        const Vec3 surf = b.v + b.w.cross(rC);              // contact velocity
        const Vec3 st = surf - nC * surf.dot(nC);           // tangential slip
        const double sm = st.norm();

        Vec3 imp = nC * (-dP);                              // normal reaction
        if (sm > 1e-12) imp += st * (-(muw * dP) / sm);     // Coulomb friction

        b.v += imp / m;
        b.w += rC.cross(imp) / I;                           // contact torque
    }

    // Non-jump regime: the raised contact deflects velocity downward; the
    // slate absorbs that vertical impulse (the ball stays on the cloth).
    // Jump shots -- where this vertical component WOULD launch the ball --
    // are the documented Airborne-state roadmap item.
    b.v.y = 0.0;
}

}  // namespace cue
