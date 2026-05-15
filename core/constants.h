#pragma once
// POOL constants (regulation 2.25 in / 6 oz). NOT snooker — the jzitelli
// reference impl uses snooker (0.1406 kg / 0.02625 m); do not copy those.
// Sources: Alciatore physical-properties FAQ; Mathavan 2010 (cushion).

namespace cue::k {

constexpr double R          = 0.028575;   // ball radius (m)  [2.25 in dia]
constexpr double M          = 0.170;      // ball mass (kg)   [6 oz]
constexpr double I_FACTOR   = 0.4;        // solid sphere: I = (2/5) m R^2
constexpr double G          = 9.81;       // gravity (m/s^2)

// Ball–cloth friction
constexpr double MU_SLIDE   = 0.20;       // sliding   (typ; range 0.15–0.40)
constexpr double MU_ROLL    = 0.013;      // rolling resistance (0.005–0.016)
constexpr double MU_SPIN    = 0.033;      // boring/spin (0.022–0.044)

// Ball–ball
constexpr double MU_BALL    = 0.05;       // speed-dependent in CP3; nominal
constexpr double E_BALL     = 0.95;       // restitution (0.92–0.98)

// Ball–cushion (Mathavan 2010). E_CUSHION is the paper's snooker-calibrated
// COR; pool cushions are lossier (Dr. Dave: ~50% speed loss perpendicular),
// so the resolver uses E_CUSHION_POOL.
constexpr double E_CUSHION      = 0.98;   // Mathavan snooker COR (reference)
constexpr double E_CUSHION_POOL = 0.85;   // fit so perpendicular retention
                                          // ~0.5 (Dr. Dave efficiency)
constexpr double MU_CUSHION     = 0.14;   // ball–rail friction
constexpr double CUSHION_H      = 1.4 * R; // contact height 7R/5 (sin=2/5)

// Ball–table (slate) — for CP5+ jump roadmap
constexpr double E_TABLE    = 0.50;       // vertical restitution (0.5–0.7)

constexpr double EPS        = 1e-9;       // generic numeric tolerance

}  // namespace cue::k
