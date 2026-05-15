#include "engine/motion.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/frame.h"

namespace cue {
namespace {

// Linearly decay a vertical spin component toward zero (never overshoot/flip).
double decaySpin(double w0y, double rate, double tau) {
    const double mag = std::fabs(w0y) - rate * tau;
    return mag <= 0.0 ? 0.0 : std::copysign(mag, w0y);
}

}  // namespace

Segment beginSegment(const Ball& b, const ClothParams& cloth) {
    Segment s;
    s.start = b;
    s.cloth = cloth;

    const Vec3 u = slip(b.v, b.w);
    const bool moving   = b.v.norm()       > kVelEps;
    const bool slipping = u.norm()         > kSlipEps;
    const bool spinning = std::fabs(b.w.y) > kSpinEps;

    const double g = k::G, R = k::R;

    if (!moving && !slipping && !spinning) {
        s.state = BallState::Stationary;
        s.T = std::numeric_limits<double>::infinity();
    } else if (!moving && !slipping) {
        s.state = BallState::Spinning;
        s.T = std::fabs(b.w.y) / (2.5 * cloth.muSpin * g / R);  // 5 mu g / 2R
    } else if (slipping) {
        s.state = BallState::Sliding;
        s.T = 2.0 * u.norm() / (7.0 * cloth.muSlide * g);
    } else {
        s.state = BallState::Rolling;
        s.T = b.v.norm() / (cloth.muRoll * g);
    }
    s.start.state = s.state;
    return s;
}

Ball Segment::at(double tau) const {
    if (tau < 0.0) tau = 0.0;
    if (tau > T)   tau = T;

    Ball b = start;
    const double g = k::G, R = k::R;

    switch (state) {
        case BallState::Stationary:
            break;

        case BallState::Spinning: {
            const double rate = 2.5 * cloth.muSpin * g / R;     // 5 mu g / 2R
            b.v = {};
            b.w = {0.0, decaySpin(start.w.y, rate, tau), 0.0};
            break;
        }

        case BallState::Sliding: {
            const Vec3 u0 = slip(start.v, start.w);
            const Vec3 uh = u0.normalized();
            const Vec3 aHalf = uh * (-0.5 * cloth.muSlide * g); // 1/2 accel
            b.r = start.r + start.v * tau + aHalf * (tau * tau);
            b.v = start.v + uh * (-cloth.muSlide * g * tau);

            const double C = 2.5 * cloth.muSlide * g / R;       // 5 mu_s g/2R
            b.w.x = start.w.x + C * tau * uh.z;
            b.w.z = start.w.z - C * tau * uh.x;
            b.w.y = decaySpin(start.w.y, 2.5 * cloth.muSpin * g / R, tau);
            break;
        }

        case BallState::Rolling: {
            const Vec3 vh = start.v.normalized();
            const Vec3 aHalf = vh * (-0.5 * cloth.muRoll * g);
            b.r = start.r + start.v * tau + aHalf * (tau * tau);
            const double speed =
                std::max(0.0, start.v.norm() - cloth.muRoll * g * tau);
            b.v = vh * speed;
            const double wy = decaySpin(
                start.w.y, (5.0 / 7.0) * cloth.muRoll * g / R, tau);
            b.w = rollingSpin(b.v, wy);                          // slaved
            break;
        }
    }
    return b;
}

Ball Segment::endBall() const {
    Ball b = at(T);
    switch (state) {
        case BallState::Sliding: {
            // No-slip invariant is exact at T; snap to kill FP drift so the
            // next phase doesn't spuriously re-enter Sliding.
            b.w = rollingSpin(b.v, b.w.y);
            b.state = (b.v.norm() > kVelEps || std::fabs(b.w.y) > kSpinEps)
                          ? BallState::Rolling
                          : BallState::Stationary;
            break;
        }
        case BallState::Rolling: {
            b.v = {};
            if (std::fabs(b.w.y) > kSpinEps) {
                b.w = {0.0, b.w.y, 0.0};
                b.state = BallState::Spinning;
            } else {
                b.w = {};
                b.state = BallState::Stationary;
            }
            break;
        }
        case BallState::Spinning:
            b.v = {};
            b.w = {};
            b.state = BallState::Stationary;
            break;
        case BallState::Stationary:
            break;
    }
    return b;
}

void Segment::positionPoly(Vec3& p0, Vec3& p1, Vec3& p2) const {
    p0 = start.r;
    p1 = {};
    p2 = {};
    const double g = k::G;
    if (state == BallState::Sliding) {
        p1 = start.v;
        p2 = slip(start.v, start.w).normalized() * (-0.5 * cloth.muSlide * g);
    } else if (state == BallState::Rolling) {
        p1 = start.v;
        p2 = start.v.normalized() * (-0.5 * cloth.muRoll * g);
    }
}

double simulateToRest(Ball b,
                      const std::function<void(double, const Ball&)>& sink,
                      double sampleDt, int maxSegments,
                      const ClothParams& cloth) {
    double tGlobal = 0.0;
    for (int seg = 0; seg < maxSegments; ++seg) {
        Segment s = beginSegment(b, cloth);
        if (s.state == BallState::Stationary) {
            if (sink) sink(tGlobal, s.start);
            break;
        }
        for (double tau = 0.0; tau < s.T; tau += sampleDt)
            if (sink) sink(tGlobal + tau, s.at(tau));
        tGlobal += s.T;
        b = s.endBall();
    }
    if (sink) sink(tGlobal, b);
    return tGlobal;
}

}  // namespace cue
