#pragma once
// ===========================================================================
// CONVENTION LOCK — read this before touching any motion/collision code.
// A sign error here propagates into every downstream gate (90/30, arcs).
// ===========================================================================
//
// Coordinate frame (right-handed):
//   x, z : horizontal cloth plane
//   y    : vertical, +y up. Cloth surface at ball-center height y = R.
//   Gravity = (0, -g, 0).
//
// Ball kinematic state: position r, linear velocity v, angular velocity w
// (omega), all in the table frame, SI units (m, m/s, rad/s).
//
// Up axis  jhat = (0,1,0).
// Contact slip velocity of the ball against the cloth (the quantity whose
// magnitude distinguishes Sliding from Rolling):
//
//     u = v + R * (jhat x w)
//
// jhat x w = (w.z, 0, -w.x), so the planar slip is
//     u = ( v.x + R*w.z , 0 , v.z - R*w.x ).
//
// Rolling-without-slip => u == 0 => w is slaved to v:
//     w = ( v.z / R , w.y , -v.x / R ).
//
// Sign of vertical spin w.y is preserved; it decays toward zero (never flips).
// ===========================================================================
#include "core/constants.h"
#include "core/vec3.h"

namespace cue {

inline Vec3 gravityVec() { return {0.0, -k::G, 0.0}; }

// Planar contact-slip velocity u = v + R (jhat x w).
inline Vec3 slip(const Vec3& v, const Vec3& w) {
    return {v.x + k::R * w.z, 0.0, v.z - k::R * w.x};
}

// Angular velocity consistent with pure rolling at linear velocity v
// (vertical spin w_y carried through unchanged).
inline Vec3 rollingSpin(const Vec3& v, double wy) {
    return {v.z / k::R, wy, -v.x / k::R};
}

// Translational + rotational kinetic energy. I = (2/5) m R^2.
inline double kineticEnergy(const Vec3& v, const Vec3& w) {
    const double I = k::I_FACTOR * k::M * k::R * k::R;
    return 0.5 * k::M * v.norm2() + 0.5 * I * w.norm2();
}

}  // namespace cue
