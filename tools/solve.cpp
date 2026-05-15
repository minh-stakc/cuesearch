// Plan a 9-ball runout from a real table layout.
//
//   solve  mytable.txt          (or: solve -   to read stdin)
//
// Layout format (engine/layout.h):
//   B <id> <C|S|T|E|O> <x> <z>      x in [0,2.54], z in [0,1.27], metres
//
// For each shot it prints a human description, executes it deterministically
// (no execution noise), then prints the table after the shot.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/layout.h"
#include "solver/plan.h"

using namespace cue;

static const char* pocketName(int i) {
    switch (i) {
        case 0: return "bottom-left corner";
        case 1: return "bottom-right corner";
        case 2: return "top-left corner";
        case 3: return "top-right corner";
        case 4: return "bottom side";
        case 5: return "top side";
    }
    return "?";
}

static std::string spinDesc(double a, double b) {
    const double R = k::R;
    std::ostringstream s;
    if (b > 0.05 * R)       s << "follow";
    else if (b < -0.05 * R) s << "draw";
    else                    s << "stun";
    if (a > 0.05 * R)       s << " + right english";
    else if (a < -0.05 * R) s << " + left english";
    char buf[64];
    std::snprintf(buf, sizeof buf, " (tip a=%.2fR b=%.2fR)", a / R, b / R);
    s << buf;
    return s.str();
}

static const char* speedDesc(double v) {
    return v < 1.6 ? "soft" : (v < 2.8 ? "medium" : "firm");
}

// Compact top-down ASCII map ('C'=cue, digits=ids, 'o'=pocket, '.'=cloth).
static void printMap(const World& w) {
    const int W = 56, H = 15;
    std::vector<std::string> g(H, std::string(W, '.'));
    auto cell = [&](double x, double z, int& c, int& r) {
        c = static_cast<int>(x / w.table.xMax * (W - 1) + 0.5);
        r = (H - 1) -
            static_cast<int>(z / w.table.zMax * (H - 1) + 0.5);  // z up
        c = c < 0 ? 0 : (c >= W ? W - 1 : c);
        r = r < 0 ? 0 : (r >= H ? H - 1 : r);
    };
    int c, r;
    for (auto& p : w.table.pockets()) { cell(p.x, p.z, c, r); g[r][c] = 'o'; }
    for (const Ball& b : w.balls) {
        if (b.pocketed) continue;
        cell(b.r.x, b.r.z, c, r);
        g[r][c] = b.type == BallType::Cue
                      ? 'C'
                      : (b.id >= 0 && b.id <= 9 ? char('0' + b.id) : '#');
    }
    std::printf("    +%s+\n", std::string(W, '-').c_str());
    for (auto& row : g) std::printf("    |%s|\n", row.c_str());
    std::printf("    +%s+\n", std::string(W, '-').c_str());
}

int main(int argc, char** argv) {
    std::string text, line;
    if (argc > 1 && std::string(argv[1]) != "-") {
        std::ifstream f(argv[1]);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
        std::ostringstream ss; ss << f.rdbuf(); text = ss.str();
    } else {
        std::ostringstream ss; ss << std::cin.rdbuf(); text = ss.str();
    }

    World w = loadLayout(text);
    int cueIx = -1;
    for (size_t i = 0; i < w.balls.size(); ++i)
        if (w.balls[i].type == BallType::Cue) cueIx = static_cast<int>(i);
    if (cueIx < 0) { std::fprintf(stderr, "no cue ball in layout\n"); return 2; }

    std::printf("Starting table:\n");
    printMap(w);

    for (int shot = 1; shot <= 12; ++shot) {
        const int tgt = legalTarget(w.balls);
        if (tgt < 0) { std::printf("\nNo object balls left.\n"); break; }

        PlanResult p = planRunout(w, 2, 24, 4, 2, 7);
        if (p.shot.targetId < 0 || p.value <= 1e-6) {
            std::printf("\nShot %d: no makeable shot on the %d "
                        "(snookered / blocked). Runout ends.\n",
                        shot, tgt);
            break;
        }

        std::printf(
            "\nShot %d: pot the %d into the %s\n"
            "  cue: %.2f m/s (%s), %s\n"
            "  est. P(pot)=%.2f   plan value (this+continuation)=%.2f\n",
            shot, p.shot.targetId, pocketName(p.shot.pocket),
            p.shot.shot.speed, speedDesc(p.shot.shot.speed),
            spinDesc(p.shot.shot.a, p.shot.shot.b).c_str(), p.shot.pPot,
            p.value);

        // Snapshot already-down balls so we report only THIS shot's pots
        // (ShotOutcome.pocketed is cumulative over the world state).
        std::vector<int> before;
        for (const Ball& b : w.balls)
            if (b.pocketed) before.push_back(b.id);
        auto wasDown = [&](int id) {
            for (int x : before) if (x == id) return true;
            return false;
        };

        cueStrike(w.balls[cueIx], p.shot.shot.aim, p.shot.shot.speed,
                  p.shot.shot.a, p.shot.shot.b);
        ShotOutcome o = simulateShot(w);

        std::vector<int> justPotted;
        for (int id : o.pocketed) if (!wasDown(id)) justPotted.push_back(id);

        std::printf("  -> ");
        if (!justPotted.empty()) {
            std::printf("potted");
            for (int id : justPotted) std::printf(" %d", id);
        } else std::printf("nothing potted");
        if (o.cueScratched) std::printf("; CUE SCRATCHED");
        std::printf("\n");
        printMap(w);

        if (o.won) { std::printf("\nRACK WON (9 potted legally).\n"); break; }
        if (o.foul != Foul::None) {
            std::printf("\nFoul -> runout ends (turn would pass).\n");
            break;
        }
        bool potted = false;
        for (int id : o.pocketed) if (id == tgt) potted = true;
        if (!potted) {
            std::printf("\nMissed the %d -> runout ends.\n", tgt);
            break;
        }
    }
    return 0;
}
