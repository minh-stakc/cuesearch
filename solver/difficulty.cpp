#include "solver/difficulty.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "engine/world.h"

namespace cue {
namespace {

constexpr double DEG = 3.14159265358979323846 / 180.0;
// Grid (ranges chosen to fit a 9-ft corner shot; lookups clamp outside).
constexpr int NA = 18;          // alpha: 0..85 deg
constexpr int ND = 11;          // dCO, dOP: 0.10..0.90 m
constexpr double A0 = 0.0,  A1 = 85.0;
constexpr double D0 = 0.10, D1 = 0.90;

double aAt(int i) { return A0 + (A1 - A0) * i / (NA - 1); }
double dAt(int i) { return D0 + (D1 - D0) * i / (ND - 1); }

// One cell's P(object potted) on CP7-validated corner geometry: object
// dOP from pocket 0 along the diagonal, cue dCO from the object at cut
// angle alpha. Counts OB pocketed (ignores cue scratch -- pure
// makeability, matching the calibrated skill curve and the value fn need).
double cellP(double alphaDeg, double dCO, double dOP, int sims) {
    const double R = k::R;
    World base;
    const Vec3 P = base.table.pockets()[0];                 // (0,0) corner
    const Vec3 out = Vec3{1, 0, 1}.normalized();            // away from it
    Vec3 O = P + out * dOP; O.y = R;
    Vec3 ghost = O + out * (2.0 * R);                       // 2R behind OB
    const double th = alphaDeg * DEG;
    Vec3 appr{out.x * std::cos(th) + out.z * std::sin(th), 0.0,
              -out.x * std::sin(th) + out.z * std::cos(th)};
    Vec3 cue = O + appr * dCO; cue.y = R;
    // Off-table -> unreachable cell; report 0 (clamped/filled by caller).
    if (cue.x < R || cue.x > base.table.xMax - R ||
        cue.z < R || cue.z > base.table.zMax - R ||
        O.x < R || O.x > base.table.xMax - R ||
        O.z < R || O.z > base.table.zMax - R)
        return -1.0;

    const Vec3 aim = (ghost - cue).normalized();
    std::mt19937 rng(7919u);
    std::normal_distribution<double> nA(0.0, k::AIM_SIGMA),
        nS(0.0, k::SPEED_SIGMA);
    int potted = 0;
    for (int s = 0; s < sims; ++s) {
        World w = base;
        Ball o; o.type = BallType::Object; o.id = 1; o.r = O;
        Ball c; c.type = BallType::Cue;   c.id = 0; c.r = cue;
        w.balls = {c, o};
        const double t2 = nA(rng), cc = std::cos(t2), ss = std::sin(t2);
        Vec3 a{aim.x * cc + aim.z * ss, 0.0, -aim.x * ss + aim.z * cc};
        cueStrike(w.balls[0], a, 3.0 * (1.0 + nS(rng)), 0.0, 0.0);
        ShotOutcome r = simulateShot(w);
        if (std::find(r.pocketed.begin(), r.pocketed.end(), 1) !=
            r.pocketed.end())
            ++potted;
    }
    return double(potted) / sims;
}

DifficultyTable g_tab;

}  // namespace

void DifficultyTable::buildOrLoad(const std::string& cacheFile,
                                  int simsPerCell) {
    grid.assign(NA * ND * ND, 0.0);
    {
        std::ifstream f(cacheFile, std::ios::binary);
        int a, d;
        if (f && (f.read(reinterpret_cast<char*>(&a), sizeof a),
                  f.read(reinterpret_cast<char*>(&d), sizeof d), f) &&
            a == NA && d == ND) {
            f.read(reinterpret_cast<char*>(grid.data()),
                   grid.size() * sizeof(double));
            if (f) { ready = true; return; }
        }
    }
    for (int i = 0; i < NA; ++i)
        for (int j = 0; j < ND; ++j) {
            double lastValid = 0.0;
            for (int k = 0; k < ND; ++k) {       // increasing dOP
                double p = cellP(aAt(i), dAt(j), dAt(k), simsPerCell);
                if (p < 0.0) p = lastValid;       // off-table: monotone fill
                else lastValid = p;
                grid[(i * ND + j) * ND + k] = p;
            }
        }
    ready = true;
    std::ofstream f(cacheFile, std::ios::binary);
    if (f) {
        int a = NA, d = ND;
        f.write(reinterpret_cast<const char*>(&a), sizeof a);
        f.write(reinterpret_cast<const char*>(&d), sizeof d);
        f.write(reinterpret_cast<const char*>(grid.data()),
                grid.size() * sizeof(double));
    }
}

double DifficultyTable::potProb(double alphaDeg, double dCO,
                                double dOP) const {
    if (!ready || grid.empty()) return 0.0;
    auto fr = [](double v, double v0, double v1, int n, int& i0) {
        double t = (v - v0) / (v1 - v0) * (n - 1);
        t = std::max(0.0, std::min(double(n - 1), t));
        i0 = std::min(n - 2, int(t));
        return t - i0;                            // frac in [0,1]
    };
    int ia, jc, kp;
    double fa = fr(alphaDeg, A0, A1, NA, ia);
    double fc = fr(dCO, D0, D1, ND, jc);
    double fp = fr(dOP, D0, D1, ND, kp);
    auto G = [&](int i, int j, int k) {
        return grid[(i * ND + j) * ND + k];
    };
    double v = 0.0;
    for (int di = 0; di < 2; ++di)
        for (int dj = 0; dj < 2; ++dj)
            for (int dk = 0; dk < 2; ++dk) {
                double w = (di ? fa : 1 - fa) * (dj ? fc : 1 - fc) *
                           (dk ? fp : 1 - fp);
                v += w * G(ia + di, jc + dj, kp + dk);
            }
    return v;
}

const DifficultyTable& difficulty() { return g_tab; }

}  // namespace cue
