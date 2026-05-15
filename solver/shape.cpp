#include "solver/shape.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/constants.h"
#include "engine/cuestrike.h"

namespace cue {
namespace {

constexpr double DEG = 3.14159265358979323846 / 180.0;

Vec3 planar(Vec3 v) { v.y = 0.0; return v; }
double R2() { return 2.0 * k::R; }

int idxOfId(const std::vector<Ball>& b, int id) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].id == id && !b[i].pocketed) return (int)i;
    return -1;
}
int cueIdx(const std::vector<Ball>& b) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].type == BallType::Cue) return (int)i;
    return -1;
}
Vec3 ghostOf(const Vec3& O, const Vec3& P) {
    return O - planar(P - O).normalized() * R2();
}
// Is segment a->b blocked by any object ball other than ids x,y?
bool blocked(const World& w, const Vec3& a, const Vec3& b, int x, int y) {
    Vec3 ab = planar(b - a);
    double L = ab.norm();
    if (L < 1e-9) return false;
    Vec3 d = ab / L;
    for (const Ball& o : w.balls) {
        if (o.pocketed || o.id == x || o.id == y) continue;
        if (o.type == BallType::Cue && (x == 0 || y == 0)) continue;
        Vec3 ao = planar(o.r - a);
        double t = std::max(0.0, std::min(L, ao.dot(d)));
        Vec3 c = a + d * t;
        if (planar(o.r - c).norm() < 1.8 * k::R) return true;   // clipped
    }
    return false;
}
double angleBetween(const Vec3& u, const Vec3& v) {
    double c = planar(u).normalized().dot(planar(v).normalized());
    c = c < -1 ? -1 : (c > 1 ? 1 : c);
    return std::acos(c);
}
int nextLegal(const World& w, int afterId) {
    int best = std::numeric_limits<int>::max();
    for (const Ball& b : w.balls)
        if (b.type != BallType::Cue && !b.pocketed && b.id != afterId)
            best = std::min(best, b.id);
    return best == std::numeric_limits<int>::max() ? -1 : best;
}

}  // namespace

double posShapeValue(const World& w) {
    const int t = legalTarget(w.balls);
    if (t < 0) return 1.0;                         // rack cleared = ideal
    const int ci = cueIdx(w.balls), ti = idxOfId(w.balls, t);
    if (ci < 0 || ti < 0) return 0.0;
    const Vec3 cue = w.balls[ci].r, O = w.balls[ti].r;
    double best = 0.0;
    for (const Vec3& P : w.table.pockets()) {
        const Vec3 g = ghostOf(O, P);
        if (blocked(w, cue, g, 0, t)) continue;     // cue path
        if (blocked(w, O, P, t, -999)) continue;    // object path
        const double cut = angleBetween(g - cue, P - O);
        if (cut > 75.0 * DEG) continue;
        const double dist = planar(g - cue).norm() + planar(P - O).norm();
        const double cc = std::max(0.0, std::cos(cut));
        best = std::max(best, cc * cc * std::exp(-dist / 1.5));
    }
    return best;                                   // 0 .. ~1
}

ShapeResult shapeShot(const World& w, unsigned seed) {
    ShapeResult out;
    out.leaveValue = -1.0;
    out.potsTarget = false;

    const int ci = cueIdx(w.balls);
    const int t = legalTarget(w.balls);
    if (ci < 0 || t < 0) return out;
    const Vec3 cue0 = w.balls[ci].r, O = w.balls[idxOfId(w.balls, t)].r;

    // Feasible pockets for the legal ball (ghost-ball geometry + LOS).
    struct Cand { Vec3 aim; };
    std::vector<Cand> pots;
    for (const Vec3& P : w.table.pockets()) {
        const Vec3 g = ghostOf(O, P);
        if (blocked(w, cue0, g, 0, t) || blocked(w, O, P, t, -999)) continue;
        if (angleBetween(g - cue0, P - O) > 75.0 * DEG) continue;
        pots.push_back({planar(g - cue0).normalized()});
    }
    if (pots.empty()) return out;

    // Target leave zones: positions giving a straight-ish shot on the NEXT
    // legal ball into each of its pockets (a few standoff distances).
    const int t2 = nextLegal(w, t);
    std::vector<Vec3> leaves;
    if (t2 >= 0) {
        const Vec3 O2 = w.balls[idxOfId(w.balls, t2)].r;
        for (const Vec3& P2 : w.table.pockets()) {
            if (blocked(w, O2, P2, t2, -999)) continue;
            const Vec3 g2 = ghostOf(O2, P2);
            Vec3 back = planar(g2 - O2).normalized();
            for (double Ld : {0.30, 0.55})
                leaves.push_back(g2 + back * Ld);
        }
    }
    if (leaves.empty()) leaves.push_back(O);        // no next ball: any leave

    // Deterministic (noiseless) shot evaluation.
    auto sim = [&](const Vec3& aim, double sp, double b, World& after) {
        World c = w;
        const int legalBefore = legalTarget(c.balls);
        cueStrike(c.balls[ci], aim, sp, 0.0, b);
        ShotOutcome o = simulateShot(c);
        after = c;
        bool potted = false;
        for (int id : o.pocketed)
            if (id == legalBefore) potted = true;
        return potted && o.foul == Foul::None;
    };

    double bestScore = -1.0;
    for (const Cand& cd : pots) {
        for (const Vec3& target : leaves) {
            // Coordinate descent on (speed, vertical spin) so the cue
            // leaves near `target` -- the shot-shaper "solves for the leave".
            double sp = 2.2, b = 0.0, step = 0.6;
            for (int iter = 0; iter < 5; ++iter) {
                double bestCost = 1e9, bSp = sp, bB = b;
                for (double ds : {-step, 0.0, step})
                    for (double db : {-0.2 * k::R, 0.0, 0.2 * k::R}) {
                        double s = std::min(3.8, std::max(1.3, sp + ds));
                        double bb = std::min(0.4 * k::R,
                                             std::max(-0.4 * k::R, b + db));
                        World after;
                        bool pot = sim(cd.aim, s, bb, after);
                        double cost =
                            pot ? planar(after.balls[ci].r - target).norm()
                                : 5.0;
                        if (cost < bestCost) {
                            bestCost = cost; bSp = s; bB = bb;
                        }
                    }
                sp = bSp; b = bB; step *= 0.5;
            }
            World after;
            bool pot = sim(cd.aim, sp, b, after);
            double score = pot ? posShapeValue(after) : -1.0;
            if (score > bestScore) {
                bestScore = score;
                out.shot = {{cd.aim, sp, 0.0, b}, t, -1,
                            ShotKind::Direct, -1, 0.0};
                out.leaveValue = pot ? posShapeValue(after) : 0.0;
                out.potsTarget = pot;
            }
        }
    }
    (void)seed;
    return out;
}

PlanShapeResult planShape(const World& w, int depth, unsigned seed) {
    PlanShapeResult pr;
    pr.value = 0.0;
    pr.potsTarget = false;
    if (legalTarget(w.balls) < 0) { pr.value = 1.0; return pr; }

    ShapeResult s = shapeShot(w, seed);
    pr.shot = s.shot;
    pr.potsTarget = s.potsTarget;
    if (!s.potsTarget) return pr;                  // can't pot the legal ball

    // Follow the modal (noiseless) leave and recurse on the next ball.
    World after = w;
    int ci = cueIdx(after.balls);
    cueStrike(after.balls[ci], s.shot.shot.aim, s.shot.shot.speed,
              s.shot.shot.a, s.shot.shot.b);
    simulateShot(after);

    if (legalTarget(after.balls) < 0) {
        pr.value = 1.0;                            // chain cleared the rack
    } else if (depth <= 1) {
        pr.value = posShapeValue(after);           // depth budget reached
    } else {
        pr.value = planShape(after, depth - 1, seed + 1).value;
    }
    return pr;
}

}  // namespace cue
