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
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/layout.h"
#include "solver/winsolve.h"

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

static const char* railName(int r) {
    switch (r) {
        case 0: return "left rail";
        case 1: return "right rail";
        case 2: return "bottom rail";
        case 3: return "top rail";
    }
    return "rail";
}

static std::string methodDesc(const ShotEval& s) {
    std::ostringstream o;
    if (s.kind == ShotKind::Bank)
        o << "BANK off the " << railName(s.rail) << " into the "
          << pocketName(s.pocket);
    else if (s.kind == ShotKind::Kick)
        o << "KICK off the " << railName(s.rail) << " (snooker escape) at the "
          << pocketName(s.pocket);
    else if (s.kind == ShotKind::Combo)
        o << "COMBO the " << s.targetId << " into the 9 -> the "
          << pocketName(s.pocket) << " (INSTANT WIN)";
    else if (s.kind == ShotKind::Carom)
        o << "CAROM off the " << s.targetId << " into the 9 -> the "
          << pocketName(s.pocket) << " (INSTANT WIN)";
    else if (s.kind == ShotKind::Safety)
        o << "play SAFE — legal contact, no pot, deny the opponent"
          << (s.rail >= 0 ? std::string(" (kick off the ") + railName(s.rail) +
                                ")"
                          : "");
    else
        o << "directly into the " << pocketName(s.pocket);
    return o.str();
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
    std::string text, path;
    int foulsOnMe = 0;                 // consecutive fouls already against me
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--fouls-on-me" && i + 1 < argc) foulsOnMe = std::atoi(argv[++i]);
        else path = a;
    }
    if (!path.empty() && path != "-") {
        std::ifstream f(path);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); return 2; }
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

    int noProgress = 0;
    for (int shot = 1; shot <= 15; ++shot) {
        const int tgt = legalTarget(w.balls);
        if (tgt < 0) { std::printf("\nNo object balls left.\n"); break; }

        WinPlan p = planWin(w, foulsOnMe, 14, 3, 7);
        if (p.shot.targetId < 0) {
            std::printf("\nNo legal shot available. Stopping.\n");
            break;
        }

        std::printf(
            "\nShot %d (on %d foul%s): %s\n"
            "  cue: %.2f m/s (%s), %s\n"
            "  est. P(pot)=%.2f   P(win the rack)=%.2f\n",
            shot, foulsOnMe, foulsOnMe == 1 ? "" : "s",
            ((p.shot.kind == ShotKind::Safety ||
              p.shot.kind == ShotKind::Combo ||
              p.shot.kind == ShotKind::Carom)
                 ? methodDesc(p.shot)
                 : "pot the " + std::to_string(p.shot.targetId) + " " +
                       methodDesc(p.shot)).c_str(),
            p.shot.shot.speed, speedDesc(p.shot.shot.speed),
            spinDesc(p.shot.shot.a, p.shot.shot.b).c_str(), p.shot.pPot,
            p.winProb);

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

        bool potted = false;
        for (int id : o.pocketed) if (id == tgt) potted = true;

        if (o.foul != Foul::None) {
            ++foulsOnMe;
            if (foulsOnMe >= 3) {
                std::printf("\n  (3rd consecutive foul — rack LOST in a real "
                            "game.)\n");
                break;
            }
            std::printf("\n  (foul — opponent would get ball-in-hand; on %d "
                        "foul%s. Continuing the solved line.)\n",
                        foulsOnMe, foulsOnMe == 1 ? "" : "s");
        } else if (p.shot.kind == ShotKind::Safety) {
            std::printf("\n  (safety — legal contact, no pot intended; turn "
                        "would pass. Continuing the solved line.)\n");
            foulsOnMe = 0;
        } else if (potted) {
            std::printf("\n  (potted the %d — continue.)\n", tgt);
            foulsOnMe = 0;
        } else {
            std::printf("\n  (missed the %d, legal — turn would pass.)\n",
                        tgt);
            foulsOnMe = 0;
        }

        // No-progress guard: a deterministic shot that pots nothing and
        // leaves the legal target unchanged would just repeat forever.
        const bool progressed = potted || legalTarget(w.balls) != tgt;
        noProgress = progressed ? 0 : noProgress + 1;
        if (noProgress >= 2) {
            std::printf("\nPosition cannot be progressed further by the "
                        "planner (the line above repeats). Stopping.\n");
            break;
        }
    }
    return 0;
}
