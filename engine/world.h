#pragma once
// Event-driven multi-ball scheduler. All balls rebased to a common clock at
// every event; next event = argmin over {phase-end, ball-cushion,
// ball-ball} with a deterministic tie-break. Trajectories are exact
// closed-form between events (no global timestep).
//
// CP2 SCOPE: detection + scheduling only. Ball-ball and cushion RESOLUTION
// are clearly-labelled PLACEHOLDERS (equal-mass elastic / normal reflection,
// NO throw, NO spin coupling). Real physics: Mathavan-2014 -> CP3,
// Mathavan-2010 cushion -> CP5.
#include <functional>
#include <vector>

#include "engine/ball.h"
#include "engine/motion.h"
#include "engine/table.h"

namespace cue {

enum class EventType { PhaseEnd, Cushion, BallBall };

struct WorldEvent {
    double t = 0.0;          // global time
    EventType type = EventType::PhaseEnd;
    int i = -1, j = -1;      // ball indices (j = -1 unless BallBall)
    int rail = -1;           // 0:xMin 1:xMax 2:zMin 3:zMax (Cushion only)
};

class World {
public:
    Table table;
    std::vector<Ball> balls;

    // Run to rest (or caps). sink(t, event, balls) fires at each event.
    // Returns total simulated time. Deterministic / bitwise-reproducible.
    double simulate(
        const std::function<void(double, const WorldEvent&,
                                 const std::vector<Ball>&)>& sink = nullptr,
        int maxEvents = 4096, double horizon = 120.0);
};

}  // namespace cue
