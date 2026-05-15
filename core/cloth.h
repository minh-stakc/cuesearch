#pragma once
// Cloth friction as a first-class, per-table parameter (NOT a hardcoded
// constant). Your table observation -> the solver must condition on cloth
// speed; the curve window scales ~ 1/muSlide. Defaults = standard cloth.
#include "core/constants.h"

namespace cue {

struct ClothParams {
    double muSlide = k::MU_SLIDE;   // sliding friction (smaller = faster cloth)
    double muRoll  = k::MU_ROLL;    // rolling resistance
    double muSpin  = k::MU_SPIN;    // boring/spin friction
};

// Tournament cloth (Simonis-like) is "fast" = lower friction; bar-box
// napped cloth is "slow" = higher friction.
inline ClothParams fastCloth() { return {0.15, 0.009, 0.022}; }
inline ClothParams slowCloth() { return {0.30, 0.018, 0.044}; }

}  // namespace cue
