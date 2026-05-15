#pragma once
// Closed-form single-ball motion (Leckie & Greenspan). No collisions yet
// (CP2). Each phase is an exact analytic Segment; the stepper chains phases
// to rest. Correctness keystone — see core/frame.h for conventions.
#include <functional>

#include "engine/ball.h"

namespace cue {

// Velocity/slip/spin thresholds for state classification (deterministic).
constexpr double kVelEps  = 1e-7;
constexpr double kSlipEps = 1e-7;
constexpr double kSpinEps = 1e-7;

struct Segment {
    BallState state = BallState::Stationary;
    double T = 0.0;     // phase duration (s); +inf for Stationary
    Ball start;         // ball at tau = 0, with .state == state

    Ball at(double tau) const;   // ball at local time tau in [0, T]
    Ball endBall() const;        // at(T), next state derived + invariant-snapped

    // Position as a quadratic in local time: r(tau) = p0 + p1*tau + p2*tau^2.
    // (p1=p2=0 for Spinning/Stationary.) Used for cross-ball event prediction.
    void positionPoly(Vec3& p0, Vec3& p1, Vec3& p2) const;
};

// Classify (v,w) -> the active phase Segment beginning at b.
Segment beginSegment(const Ball& b);

// Advance a single ball through its full motion sequence to rest.
// sink(tGlobal, ball) is called with densely sampled states (for tests/viz).
// Returns total time to rest.
double simulateToRest(Ball b,
                      const std::function<void(double, const Ball&)>& sink,
                      double sampleDt = 1e-3, int maxSegments = 64);

}  // namespace cue
