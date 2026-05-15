#include "engine/world.h"

#include <array>
#include <cmath>
#include <limits>

#include "core/constants.h"
#include "core/frame.h"
#include "engine/resolve_ballball.h"
#include "math/poly_solvers.h"

namespace cue {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kTimeEps = 1e-9;

bool moving(BallState s) {
    return s == BallState::Sliding || s == BallState::Rolling;
}

// Smallest root of a*s^2+b*s+c in (eps, hi].
double smallestQuadIn(double a, double b, double c, double hi) {
    double best = kInf;
    for (double r : poly::quadratic(a, b, c))
        if (r > kTimeEps && r <= hi && r < best) best = r;
    return best;
}

// Earlier event wins; exact ties broken by type then indices (determinism).
bool earlier(const WorldEvent& a, const WorldEvent& b) {
    if (std::fabs(a.t - b.t) > 1e-12) return a.t < b.t;
    if (a.type != b.type) return a.type < b.type;
    if (a.i != b.i) return a.i < b.i;
    if (a.j != b.j) return a.j < b.j;
    return a.rail < b.rail;
}

}  // namespace

double World::simulate(
    const std::function<void(double, const WorldEvent&,
                             const std::vector<Ball>&)>& sink,
    int maxEvents, double horizon) {
    const int n = static_cast<int>(balls.size());
    std::vector<Segment> seg(n);
    for (int k = 0; k < n; ++k) seg[k] = beginSegment(balls[k], cloth);

    double now = 0.0;

    for (int ev = 0; ev < maxEvents; ++ev) {
        // Position polynomials (local time s, anchored at `now`).
        std::vector<Vec3> p0(n), p1(n), p2(n);
        for (int k = 0; k < n; ++k) seg[k].positionPoly(p0[k], p1[k], p2[k]);

        WorldEvent best;
        best.t = kInf;
        bool have = false;
        const auto consider = [&](WorldEvent e) {
            if (!have || earlier(e, best)) { best = e; have = true; }
        };

        for (int k = 0; k < n; ++k) {
            if (seg[k].state == BallState::Stationary) continue;

            // Phase end.
            consider({now + seg[k].T, EventType::PhaseEnd, k, -1, -1});

            if (!moving(seg[k].state)) continue;

            // Cushions: ball-center coordinate reaches rail-inset line.
            const double win = seg[k].T;
            const std::array<double, 4> railVal{
                table.cxMin(), table.cxMax(), table.czMin(), table.czMax()};
            for (int rl = 0; rl < 4; ++rl) {
                const bool xr = (rl < 2);
                const double c0 = (xr ? p0[k].x : p0[k].z) - railVal[rl];
                const double c1 = xr ? p1[k].x : p1[k].z;
                const double c2 = xr ? p2[k].x : p2[k].z;
                const double s = smallestQuadIn(c2, c1, c0, win);
                if (s < kInf)
                    consider({now + s, EventType::Cushion, k, -1, rl});
            }
        }

        // Ball-ball: |D0 + D1 s + D2 s^2|^2 = (2R)^2  (quartic in s).
        const double dd = (2.0 * k::R) * (2.0 * k::R);
        for (int a = 0; a < n; ++a) {
            if (!moving(seg[a].state)) continue;
            for (int b = a + 1; b < n; ++b) {
                if (seg[b].state == BallState::Stationary && !moving(seg[a].state))
                    continue;
                const Vec3 D0 = p0[a] - p0[b];
                const Vec3 D1 = p1[a] - p1[b];
                const Vec3 D2 = p2[a] - p2[b];
                std::array<double, 5> q{
                    D2.dot(D2), 2.0 * D2.dot(D1),
                    D1.dot(D1) + 2.0 * D2.dot(D0), 2.0 * D1.dot(D0),
                    D0.dot(D0) - dd};
                const double win = std::min(seg[a].T, seg[b].T);
                const double s = poly::smallestRootIn(q, 0.0, win);
                if (!std::isnan(s) && s > kTimeEps) {
                    // Must be approaching (radial closing) at contact.
                    const Vec3 d = D0 + D1 * s + D2 * (s * s);
                    const Vec3 dv = D1 + D2 * (2.0 * s);
                    if (d.dot(dv) < 0.0)
                        consider({now + s, EventType::BallBall, a, b, -1});
                }
            }
        }

        if (!have || best.t > horizon) break;

        const double s = best.t - now;
        now = best.t;
        // Rebase every ball to `now` (exact; closed-form re-anchor).
        for (int k = 0; k < n; ++k) {
            balls[k] = seg[k].at(s > seg[k].T ? seg[k].T : s);
        }

        // Apply the event (PLACEHOLDER resolutions — replaced CP3/CP5).
        switch (best.type) {
            case EventType::PhaseEnd:
                balls[best.i] = seg[best.i].endBall();
                break;
            case EventType::Cushion: {
                Ball& bl = balls[best.i];
                if (best.rail < 2) bl.v.x = -k::E_CUSHION * bl.v.x;
                else               bl.v.z = -k::E_CUSHION * bl.v.z;
                break;
            }
            case EventType::BallBall:
                resolveBallBall(balls[best.i], balls[best.j]);  // CP3
                break;
        }
        for (int k = 0; k < n; ++k) seg[k] = beginSegment(balls[k], cloth);

        if (sink) sink(now, best, balls);

        bool anyMoving = false;
        for (int k = 0; k < n; ++k)
            if (seg[k].state != BallState::Stationary) anyMoving = true;
        if (!anyMoving) break;
    }
    return now;
}

}  // namespace cue
