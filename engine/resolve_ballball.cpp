// ===== INTERNALIZE =========================================================
// Line-of-centres frame (yb = A->B). Friction is purely tangential, so the
// NORMAL is decoupled: total normal impulse = (1+e) * m_reduced * v_closing
// (m_reduced = m/2 equal masses). Integrate that impulse in N steps; each
// step applies Coulomb friction -mu * dP along the contact slip and updates
// v (dP/m) and w (5/(2mR) * dP). Recompute slip each step -> THROW EMERGES
// from the accumulated tangential impulse (never hardcoded). Speed-dependent
// Marlow mu. Textbook 90deg only at e=1; e=0.95 => atan(tanphi*2/(1-e)).
// ===========================================================================
#include "engine/resolve_ballball.h"

#include <cmath>

#include "core/constants.h"

namespace cue {
namespace {

// Marlow ball-ball friction fit (Alciatore): mu(s) = a + b*exp(-c*s),
// s = tangential slip speed (m/s). Higher at low speed -> more throw slow.
constexpr double kA = 0.009, kB = 0.108, kC = 1.088;

double muBall(double slipSpeed, double override_) {
    if (override_ >= 0.0) return override_;
    return kA + kB * std::exp(-kC * slipSpeed);
}

}  // namespace

void resolveBallBall(Ball& A, Ball& B, double muOverride, double eOverride) {
    const double m = k::M, R = k::R;
    const double e = (eOverride >= 0.0) ? eOverride : k::E_BALL;
    const double gs = 5.0 / (2.0 * m * R);     // tangential impulse -> spin

    // Line-of-centres frame: yb = A->B (planar), xb tangent, zb = up.
    Vec3 nb = B.r - A.r;
    nb.y = 0.0;
    nb = nb.normalized();
    const Vec3 yb = nb;
    const Vec3 xb{-nb.z, 0.0, nb.x};
    const Vec3 zb{0.0, 1.0, 0.0};
    const auto toL = [&](const Vec3& v) {
        return Vec3{v.dot(xb), v.dot(yb), v.dot(zb)};
    };
    const auto toW = [&](const Vec3& v) {
        return xb * v.x + yb * v.y + zb * v.z;
    };

    Vec3 vA = toL(A.v), wA = toL(A.w), vB = toL(B.v), wB = toL(B.w);

    const double vn0 = vA.y - vB.y;            // closing speed along yb
    if (vn0 <= 0.0) return;                      // not approaching

    // Friction acts only tangentially, so the normal restitution alone fixes
    // the total normal impulse: P_f = (1+e) * reduced_mass * v_closing.
    const double Pf = (1.0 + e) * (0.5 * m) * vn0;
    const int N = 4000;
    const double dP = Pf / N;

    // Tangential slip of A relative to B at the contact point.
    double uCx = (vA.x - vB.x) - R * (wA.z + wB.z);
    double uCz = R * (wA.x + wB.x);

    for (int s = 0; s < N; ++s) {
        const double us = std::sqrt(uCx * uCx + uCz * uCz);
        double dP1 = 0.0, dP2 = 0.0;
        if (us > 1e-12) {
            const double mu = muBall(us, muOverride);
            dP1 = -mu * dP * (uCx / us);
            dP2 = -mu * dP * (uCz / us);
        }

        vA.x += dP1 / m;  vA.y += -dP / m;
        vB.x += -dP1 / m; vB.y += dP / m;
        wA.x += gs * dP2; wA.z += -gs * dP1;
        wB.x += gs * dP2; wB.z += -gs * dP1;

        const double nx = (vA.x - vB.x) - R * (wA.z + wB.z);
        const double nz = R * (wA.x + wB.x);
        // Coulomb static cap: friction cannot reverse the slip direction.
        uCx = (nx * uCx < 0.0) ? 0.0 : nx;
        uCz = (nz * uCz < 0.0) ? 0.0 : nz;
    }

    A.v = toW(vA);  A.w = toW(wA);
    B.v = toW(vB);  B.w = toW(wB);
}

}  // namespace cue
