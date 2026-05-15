// Simulate a full 9-ball match between two equal AI players.
//
//   match  table.txt              (or:  randtable | match -)
//   match  table.txt 42           fixed seed (reproducible match)
//   match  table.txt 42 --quiet   result + shot log only, no boards
//
// Both players use the same policy: pot-EV positional planner for offence,
// win-EV/safety only when snookered. Shots EXECUTE with stroke noise, so
// misses/fouls happen and turns alternate. Full 9-ball: keep shooting while
// you legally pot the lowest ball; foul -> opponent ball-in-hand; the 9
// potted legally wins; three consecutive fouls loses.
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/layout.h"
#include "solver/plan.h"
#include "solver/winsolve.h"

using namespace cue;

static void printMap(const World& w) {
    const int W = 120, H = 30;   // ~2.1 cm/char; resolves a real rack
    std::vector<std::string> g(H, std::string(W, '.'));
    auto cell = [&](double x, double z, int& c, int& r) {
        c = (int)(x / w.table.xMax * (W - 1) + 0.5);
        r = (H - 1) - (int)(z / w.table.zMax * (H - 1) + 0.5);
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

// A tight 9-ball diamond on the foot spot: 1 at the apex (toward the
// breaker), 9 dead centre, the rest random. Small per-ball jitter -> a
// realistic imperfect-but-tight rack. Cue ball in the kitchen.
// Prints the rack as the canonical 9-ball diamond so it is unambiguous
// (the match board is too coarse to resolve a real ~20 cm rack).
static void printRackDiagram(const int s[9]) {
    std::printf("        %d\n", s[0]);             // apex (toward breaker)
    std::printf("      %d   %d\n", s[1], s[2]);
    std::printf("    %d   %d   %d\n", s[3], s[4], s[5]);   // s[4]=9 (centre)
    std::printf("      %d   %d\n", s[6], s[7]);
    std::printf("        %d\n", s[8]);             // back
}

static World buildRack(std::mt19937& rng, int slotId[9]) {
    World w;
    const double R = k::R, s = 2.0 * R * 1.015;        // ~0.9 mm gaps
    const double pitch = s * 0.86602540378;            // row pitch (sqrt3/2)
    const double fx = 0.75 * w.table.xMax;             // foot spot
    const double z0 = 0.5 * w.table.zMax;
    std::uniform_real_distribution<double> J(-3e-4, 3e-4);   // <=0.3 mm

    struct Slot { double x, z; };
    std::vector<Slot> slot;
    slot.push_back({fx, z0});                                   // 0 apex (1)
    slot.push_back({fx + pitch, z0 - s / 2});                   // 1
    slot.push_back({fx + pitch, z0 + s / 2});                   // 2
    slot.push_back({fx + 2 * pitch, z0 - s});                   // 3
    slot.push_back({fx + 2 * pitch, z0});                       // 4 centre (9)
    slot.push_back({fx + 2 * pitch, z0 + s});                   // 5
    slot.push_back({fx + 3 * pitch, z0 - s / 2});               // 6
    slot.push_back({fx + 3 * pitch, z0 + s / 2});               // 7
    slot.push_back({fx + 4 * pitch, z0});                       // 8 back

    std::vector<int> rest = {2, 3, 4, 5, 6, 7, 8};
    std::shuffle(rest.begin(), rest.end(), rng);
    int id[9];
    id[0] = 1;
    id[4] = 9;
    for (int k = 0, r = 0; k < 9; ++k)
        if (k != 0 && k != 4) id[k] = rest[r++];

    for (int k = 0; k < 9; ++k) {
        Ball b;
        b.type = BallType::Object;
        b.id = id[k];
        b.r = {slot[k].x + J(rng), R, slot[k].z + J(rng)};
        w.balls.push_back(b);
        slotId[k] = id[k];
    }
    Ball cue;
    cue.type = BallType::Cue;
    cue.id = 0;
    cue.r = {0.20 * w.table.xMax, R, z0 + J(rng) * 8.0};        // kitchen
    w.balls.insert(w.balls.begin(), cue);
    return w;
}

static int cueIdx(const std::vector<Ball>& b) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].type == BallType::Cue) return (int)i;
    return -1;
}

// Coarse ball-in-hand: try cue spots behind the legal target toward each
// pocket; keep the one with the best single-shot pot prob.
static void placeBallInHand(World& w, int ci, unsigned seed) {
    const int tgt = legalTarget(w.balls);
    if (tgt < 0) return;
    Vec3 T;
    for (const Ball& b : w.balls)
        if (b.id == tgt && !b.pocketed) T = b.r;
    double best = -1.0;
    Vec3 bestPos = w.balls[ci].r;
    for (const Vec3& P : w.table.pockets()) {
        Vec3 d = P - T; d.y = 0;
        if (d.norm() < 1e-6) continue;
        d = d.normalized();
        Vec3 pos = T - d * 0.35;                      // behind T toward P
        pos.y = k::R;
        if (pos.x < w.table.cxMin() || pos.x > w.table.cxMax() ||
            pos.z < w.table.czMin() || pos.z > w.table.czMax())
            continue;
        bool clash = false;
        for (const Ball& b : w.balls)
            if (!b.pocketed && b.type != BallType::Cue &&
                (b.r - pos).norm() < 2.2 * k::R)
                clash = true;
        if (clash) continue;
        World t = w;
        t.balls[ci].r = pos;
        double pp = bestShot(t, 8, seed).pPot;
        if (pp > best) { best = pp; bestPos = pos; }
    }
    w.balls[ci].r = bestPos;
}

int main(int argc, char** argv) {
    std::string text, pathArg;
    unsigned seed = 1;
    bool quiet = false, doBreak = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--quiet") quiet = true;
        else if (a == "--break") doBreak = true;
        else if (!a.empty() && (isdigit((unsigned char)a[0])))
            seed = (unsigned)std::atoi(a.c_str());
        else pathArg = a;
    }

    std::mt19937 rng(seed);
    std::normal_distribution<double> nAim(0.0, 0.009), nSpd(0.0, 0.05);

    int player = 0;                       // 0 = Player 1, 1 = Player 2
    int fouls[2] = {0, 0};
    bool ballInHand = false;
    const char* NAME[2] = {"Player 1", "Player 2"};
    int startShot = 1;

    World w;
    int slotId[9];
    if (doBreak) {
        w = buildRack(rng, slotId);
    } else {
        if (!pathArg.empty() && pathArg != "-") {
            std::ifstream f(pathArg);
            if (!f) { std::fprintf(stderr, "cannot open %s\n", pathArg.c_str()); return 2; }
            std::ostringstream ss; ss << f.rdbuf(); text = ss.str();
        } else {
            std::ostringstream ss; ss << std::cin.rdbuf(); text = ss.str();
        }
        w = loadLayout(text);
    }
    const int ci = cueIdx(w.balls);
    if (ci < 0) { std::fprintf(stderr, "no cue ball in layout\n"); return 2; }

    std::printf("=== 9-ball match: Player 1 vs Player 2 (seed %u) ===\n", seed);

    int winner = -1;
    const int MAXSHOTS = 80;

    if (doBreak) {
        std::printf("\nRacked — tight 9-ball diamond (1 at apex toward the "
                    "breaker, 9 dead centre):\n\n");
        printRackDiagram(slotId);
        // Prove it is a real diamond, not a line: actual (x,z) by row.
        std::printf("\n  apex 1 at (%.3f, %.3f);  9 at (%.3f, %.3f);  "
                    "back %d at (%.3f, %.3f)\n",
                    w.balls[1].r.x, w.balls[1].r.z,
                    w.balls[5].r.x, w.balls[5].r.z,
                    slotId[8], w.balls[9].r.x, w.balls[9].r.z);
        if (!quiet) printMap(w);
        // Strong-but-human break: hit the 1-ball full, ~9 m/s ball speed.
        Vec3 apex = w.balls[0].r;
        for (const Ball& b : w.balls)
            if (b.id == 1) apex = b.r;
        Vec3 aim = apex - w.balls[ci].r; aim.y = 0; aim = aim.normalized();
        const double th = nAim(rng) * 0.6, c = std::cos(th), sn = std::sin(th);
        aim = {aim.x * c + aim.z * sn, 0.0, -aim.x * sn + aim.z * c};
        std::printf("\nPlayer 1 BREAKS (≈ hard amateur, not a pro smash).\n");
        cueStrike(w.balls[ci], aim, 6.0 * (1.0 + nSpd(rng)), 0.0, 0.0);
        ShotOutcome o = simulateShot(w);
        std::printf("Break -> ");
        bool any = false;
        for (int idp : o.pocketed) {
            std::printf("%spotted %d", any ? ", " : "", idp);
            any = true;
        }
        if (!any) std::printf("nothing potted");
        if (o.cueScratched) std::printf("; SCRATCH");
        std::printf("\nSpread after the break:\n");
        if (!quiet) printMap(w);

        if (o.won && o.foul == Foul::None) {
            std::printf("\n*** Player 1 WINS on the break (9 potted) ***\n");
            return 0;
        }
        if (o.foul != Foul::None) {
            std::printf("Foul on the break — Player 2 gets ball-in-hand.\n");
            player = 1;
            ballInHand = true;
        } else if (any) {
            std::printf("Legal break with a ball pocketed — Player 1 "
                        "continues.\n");
            player = 0;
        } else {
            std::printf("Dry break — turn passes to Player 2.\n");
            player = 1;
        }
        startShot = 2;
    } else if (!quiet) {
        printMap(w);
    }

    for (int shot = startShot; shot <= MAXSHOTS; ++shot) {
        const int tgt = legalTarget(w.balls);
        if (tgt < 0) { std::printf("\nNo object balls left — drawn.\n"); break; }

        if (ballInHand) {
            placeBallInHand(w, ci, seed + shot);
            std::printf("\n%s takes ball-in-hand.\n", NAME[player]);
            ballInHand = false;
        }

        // Same policy for both: offence via pot-EV, safety only if snookered.
        PlanResult pr = planRunout(w, 2, 12, 3, 2, seed + shot);
        ShotEval s;
        bool safety = false;
        if (pr.shot.targetId >= 0 && pr.shot.pPot > 0.05) {
            s = pr.shot;
        } else {
            WinPlan wp = planWin(w, fouls[player], 12, 3, seed + shot);
            if (wp.shot.targetId < 0) {
                std::printf("\n%s has no legal shot — drawn.\n", NAME[player]);
                break;
            }
            s = wp.shot;
            safety = true;
        }

        std::printf("\nShot %d  %s  (fouls: P1=%d P2=%d)  legal ball: %d\n",
                    shot, NAME[player], fouls[0], fouls[1], tgt);
        std::printf("  %s%s  @ %.2f m/s\n",
                    safety ? "" : "pot ", safety ? "SAFETY" : "the shot",
                    s.shot.speed);

        // Execute WITH stroke noise (this is what makes it a real match).
        const double th = nAim(rng), c = std::cos(th), sn = std::sin(th);
        Vec3 aim{s.shot.aim.x * c + s.shot.aim.z * sn, 0.0,
                 -s.shot.aim.x * sn + s.shot.aim.z * c};
        std::vector<int> before;
        for (const Ball& b : w.balls) if (b.pocketed) before.push_back(b.id);
        auto wasDown = [&](int id) {
            for (int x : before)
                if (x == id) return true;
            return false;
        };
        cueStrike(w.balls[ci], aim, s.shot.speed * (1.0 + nSpd(rng)),
                  s.shot.a, s.shot.b);
        ShotOutcome o = simulateShot(w);

        std::printf("  -> ");
        bool any = false;
        for (int id : o.pocketed)
            if (!wasDown(id)) { std::printf("%spotted %d", any ? ", " : "", id); any = true; }
        if (!any) std::printf("nothing potted");
        if (o.cueScratched) std::printf("; SCRATCH");
        std::printf("\n");
        if (!quiet) printMap(w);

        if (o.won && o.foul == Foul::None) {
            winner = player;
            std::printf("\n*** %s WINS — potted the 9 legally on shot %d ***\n",
                        NAME[player], shot);
            break;
        }
        if (o.foul != Foul::None) {
            if (++fouls[player] >= 3) {
                winner = 1 - player;
                std::printf("\n*** %s WINS — %s's 3rd consecutive foul ***\n",
                            NAME[1 - player], NAME[player]);
                break;
            }
            std::printf("  foul (%s now on %d). %s gets ball-in-hand.\n",
                        NAME[player], fouls[player], NAME[1 - player]);
            player = 1 - player;
            ballInHand = true;
            continue;
        }
        fouls[player] = 0;                              // legal shot clears
        bool potted = false;
        for (int id : o.pocketed) if (id == tgt) potted = true;
        if (potted && !safety) {
            std::printf("  legal pot — %s continues.\n", NAME[player]);
        } else {
            std::printf("  %s — turn passes to %s.\n",
                        safety ? "safety" : "miss", NAME[1 - player]);
            player = 1 - player;
        }
    }
    if (winner < 0)
        std::printf("\nCalled after %d shots (safety battle / no result).\n",
                    MAXSHOTS);
    return 0;
}
