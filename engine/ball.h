#pragma once
#include "core/vec3.h"

namespace cue {

// Four motion states (Leckie & Greenspan). Transitions are exact closed-form
// times; a collision event (CP2+) can preempt any non-terminal state.
enum class BallState { Stationary, Spinning, Sliding, Rolling };

// Identity for the game-state / solver layers (CP6+). Defaults keep every
// existing physics test unchanged.
enum class BallType { Cue, Solid, Stripe, Eight, Object };

struct Ball {
    Vec3 r;   // position (center), table frame, y = R on the cloth
    Vec3 v;   // linear velocity
    Vec3 w;   // angular velocity (omega)
    BallState state = BallState::Stationary;

    int id = 0;
    BallType type = BallType::Object;
    bool pocketed = false;
};

inline const char* stateName(BallState s) {
    switch (s) {
        case BallState::Stationary: return "Stationary";
        case BallState::Spinning:   return "Spinning";
        case BallState::Sliding:    return "Sliding";
        case BallState::Rolling:    return "Rolling";
    }
    return "?";
}

}  // namespace cue
