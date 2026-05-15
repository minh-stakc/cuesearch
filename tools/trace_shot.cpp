// Emit a JSON trace of a shot for the visualizer.
//   trace_shot follow  > follow.json
//   trace_shot runout  > runout.json
//   trace_shot -        < layout.txt > shot.json   (load a real layout; the
//                                                    cue's v,w must be set by
//                                                    a prior strike -- here we
//                                                    just roll it gently)
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/layout.h"
#include "solver/plan.h"

using namespace cue;

static void emit(World& w) {
    const double R = k::R;
    std::ostringstream js;
    js.precision(5);
    js << "{\"R\":" << R << ",\"table\":{\"xMax\":" << w.table.xMax
       << ",\"zMax\":" << w.table.zMax << ",\"pr\":" << w.table.pocketR
       << "},\"frames\":[";
    bool first = true;
    w.trace(
        [&](double t, const std::vector<Ball>& bs) {
            if (!first) js << ',';
            first = false;
            js << "{\"t\":" << t << ",\"b\":[";
            bool fb = true;
            for (const Ball& b : bs) {
                if (b.pocketed) continue;
                if (!fb) js << ',';
                fb = false;
                js << "{\"id\":" << b.id << ",\"x\":" << b.r.x
                   << ",\"z\":" << b.r.z << "}";
            }
            js << "]}";
        },
        0.008);
    js << "]}";
    std::cout << js.str() << "\n";
}

int main(int argc, char** argv) {
    const double R = k::R;
    std::string mode = argc > 1 ? argv[1] : "follow";
    World w;

    if (mode == "runout") {
        const Vec3 P0 = w.table.pockets()[0];
        const Vec3 P2 = w.table.pockets()[2];
        Ball one; one.type = BallType::Object; one.id = 1;
        one.r = P0 + Vec3{1, 0, 1}.normalized() * 0.10;
        Ball nine; nine.type = BallType::Object; nine.id = 9;
        nine.r = P2 + Vec3{1, 0, -1}.normalized() * 0.10;
        Ball c; c.type = BallType::Cue; c.id = 0; c.r = {1.0, R, 0.63};
        w.balls = {c, one, nine};
        PlanResult p = planRunout(w, 2, 24, 4, 2, 3);
        cueStrike(w.balls[0], p.shot.shot.aim, p.shot.shot.speed,
                  p.shot.shot.a, p.shot.shot.b);
    } else {  // "follow": cue cuts a ball with topspin -> post-collision arc
        Ball b; b.type = BallType::Object; b.id = 1; b.r = {1.4, R, 0.6};
        Ball c; c.type = BallType::Cue; c.id = 0; c.r = {0.5, R, 0.45};
        w.balls = {c, b};
        cueStrike(w.balls[0], Vec3{1, 0, 0.12}.normalized(), 3.0, 0.0,
                  0.35 * R);  // follow
    }
    emit(w);
    return 0;
}
