#include "solver/runout.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "solver/difficulty.h"

namespace cue {
namespace {

constexpr double DEG = 3.14159265358979323846 / 180.0;
Vec3 planar(Vec3 v) { v.y = 0.0; return v; }

int cueIdx(const std::vector<Ball>& b) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].type == BallType::Cue) return (int)i;
    return -1;
}
int idxOfId(const std::vector<Ball>& b, int id) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].id == id && !b[i].pocketed) return (int)i;
    return -1;
}
Vec3 ghostOf(const Vec3& O, const Vec3& P) {
    return O - planar(P - O).normalized() * (2.0 * k::R);
}
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
        if (planar(o.r - (a + d * t)).norm() < 1.8 * k::R) return true;
    }
    return false;
}
double angBetween(const Vec3& u, const Vec3& v) {
    double c = planar(u).normalized().dot(planar(v).normalized());
    c = c < -1 ? -1 : (c > 1 ? 1 : c);
    return std::acos(c);
}
// Sorted (ascending id) non-pocketed object balls = the legal order.
std::vector<int> legalOrder(const World& w) {
    std::vector<int> ids;
    for (const Ball& b : w.balls)
        if (b.type != BallType::Cue && !b.pocketed) ids.push_back(b.id);
    std::sort(ids.begin(), ids.end());
    return ids;
}
// Best table P(pot) for object `oi` from cue position `cue` (LOS-gated).
double bestPot(const World& w, const Vec3& cue, int oid) {
    const int oi = idxOfId(w.balls, oid);
    if (oi < 0) return 0.0;
    const Vec3 O = w.balls[oi].r;
    double best = 0.0;
    for (const Vec3& P : w.table.pockets()) {
        const Vec3 g = ghostOf(O, P);
        if (blocked(w, cue, g, 0, oid)) continue;
        if (blocked(w, O, P, oid, -999)) continue;
        const double alpha = angBetween(g - cue, P - O) / DEG;
        const double dCO = planar(g - cue).norm();
        const double dOP = planar(P - O).norm();
        best = std::max(best, difficulty().potProb(alpha, dCO, dOP));
    }
    return best;
}
// Cue-independent "openness" of object `oid` (any clear pocket, neutral
// angle) -- the coarse p2/p3 term of the ordered-9-ball value.
double openness(const World& w, int oid) {
    const int oi = idxOfId(w.balls, oid);
    if (oi < 0) return 0.0;
    const Vec3 O = w.balls[oi].r;
    double best = 0.0;
    for (const Vec3& P : w.table.pockets()) {
        if (blocked(w, O, P, oid, -999)) continue;
        best = std::max(best,
                        difficulty().potProb(30.0, 0.6,
                                             planar(P - O).norm()));
    }
    return best;
}

}  // namespace

double mobilityValue(const World& w) {
    const int ci = cueIdx(w.balls);
    std::vector<int> ord = legalOrder(w);
    if (ord.empty()) return 1.0;                 // rack cleared = ideal
    if (ci < 0) return 0.0;
    const Vec3 cue = w.balls[ci].r;
    // 9-ball adaptation of PickPocket/CueCard 1,0.33,0.15: ordered next-3
    // legal balls (you must shoot the lowest), p1 from the real cue.
    double p1 = bestPot(w, cue, ord[0]);
    double p2 = ord.size() > 1 ? openness(w, ord[1]) : 1.0;
    double p3 = ord.size() > 2 ? openness(w, ord[2]) : 1.0;
    return 1.0 * p1 + 0.33 * p2 + 0.15 * p3;
}

LeaveShot seedLeaveShot(const World& w, int targetId, int pocketIdx,
                        const Vec3& leave) {
    LeaveShot out;
    const int ci = cueIdx(w.balls);
    const int oi = idxOfId(w.balls, targetId);
    if (ci < 0 || oi < 0 || pocketIdx < 0 || pocketIdx > 5) return out;
    const Vec3 cue0 = w.balls[ci].r;
    const Vec3 O = w.balls[oi].r;
    const Vec3 P = w.table.pockets()[pocketIdx];
    const Vec3 dOP = planar(P - O).normalized();
    const Vec3 ghost = O - dOP * (2.0 * k::R);
    if (blocked(w, cue0, ghost, 0, targetId) ||
        blocked(w, O, P, targetId, -999))
        return out;
    if (angBetween(ghost - cue0, dOP) > 80.0 * DEG) return out;

    const Vec3 aim = planar(ghost - cue0).normalized();
    // Stun departure ~ component of the cue's approach perpendicular to the
    // line of centres; follow/draw blend it toward/away from "forward".
    const Vec3 fwd = aim;
    Vec3 tHat = fwd - dOP * fwd.dot(dOP);
    if (planar(tHat).norm() < 1e-6) tHat = {-dOP.z, 0, dOP.x};   // full hit
    tHat = planar(tHat).normalized();

    const Vec3 u = planar(leave - ghost);
    const Vec3 hHat = u.norm() > 1e-6 ? u.normalized() : tHat;

    // 1-D "solve" for the blend beta (NOT a blind 5-D search): pick the
    // post-contact heading closest to the desired one.
    double bestBeta = 0.0, bestAng = 1e9;
    for (int s = -6; s <= 6; ++s) {
        double beta = s / 6.0 * 1.6;
        Vec3 h = planar(tHat + fwd * beta);
        if (h.norm() < 1e-6) continue;
        double a = angBetween(h.normalized(), hHat);
        if (a < bestAng) { bestAng = a; bestBeta = beta; }
    }
    // Speed from the inverse rolling-distance law d = v^2/(2 mu_r g),
    // de-rated for collision energy loss and the stroke->ball gain.
    double vCue = std::sqrt(2.0 * k::MU_ROLL * k::G *
                            std::max(0.05, u.norm()));
    double speed = vCue / (0.70 * 1.52);
    speed = std::max(1.1, std::min(4.5, speed));
    double b = std::max(-0.45 * k::R,
                        std::min(0.45 * k::R, bestBeta / 1.6 * 0.45 * k::R));

    auto trial = [&](double sp, double bb) {
        World c = w;
        const int legalBefore = legalTarget(c.balls);
        cueStrike(c.balls[ci], aim, sp, 0.0, bb);
        ShotOutcome o = simulateShot(c);
        bool pot = false;
        for (int id : o.pocketed) if (id == legalBefore) pot = true;
        double err = pot ? planar(c.balls[ci].r - leave).norm() : 9.0;
        return std::make_pair(pot && o.foul == Foul::None, err);
    };

    // Seeded simulate-and-correct (converges fast: the seed is close).
    double step = 0.4;
    auto cur = trial(speed, b);
    out.potsTarget = cur.first;
    out.leaveErr = cur.second;
    out.shot = {{aim, speed, 0.0, b}, targetId, pocketIdx,
                ShotKind::Direct, -1, 0.0};
    for (int it = 0; it < 4; ++it) {
        double bestS = speed, bestB = b, bestE = out.leaveErr;
        bool bestP = out.potsTarget;
        for (double ds : {-step, 0.0, step})
            for (double db : {-0.15 * k::R, 0.0, 0.15 * k::R}) {
                double sp = std::max(1.1, std::min(4.5, speed + ds));
                double bb =
                    std::max(-0.45 * k::R, std::min(0.45 * k::R, b + db));
                auto r = trial(sp, bb);
                if (r.first && (!bestP || r.second < bestE)) {
                    bestP = true; bestE = r.second; bestS = sp; bestB = bb;
                }
            }
        speed = bestS; b = bestB;
        out.potsTarget = bestP; out.leaveErr = bestE;
        out.shot.shot.speed = speed; out.shot.shot.b = b;
        step *= 0.5;
    }
    return out;
}

}  // namespace cue
