// Probe: replay the leave-B test scenario and dump cue state right
// after the ball-ball event for draw/stun/follow.
#include <cstdio>
#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "engine/world.h"
using namespace cue;

int main() {
    const double R = k::R;
    for (double b : {-0.30 * R, 0.0, +0.30 * R}) {
        World w;
        Ball cue; cue.type = BallType::Cue; cue.id = 0;
        cue.r = {0.50, R, 0.635};
        Ball obj; obj.type = BallType::Object; obj.id = 1;
        obj.r = {1.10, R, 0.635};
        w.balls = {cue, obj};
        cueStrike(w.balls[0], Vec3{1, 0, 0}, 2.2, 0.0, b);
        std::printf("===== b=%+.5f =====\n", b);
        auto evn = [](EventType t){
            switch(t){case EventType::PhaseEnd:return "PhaseEnd";
                      case EventType::Pocket:return "Pocket";
                      case EventType::Cushion:return "Cushion";
                      case EventType::BallBall:return "BallBall";}
            return "?";
        };
        w.simulate([&](double t, const WorldEvent& e,
                       const std::vector<Ball>& bs) {
            std::printf("  t=%.4f %-8s i=%d j=%d rail=%d  "
                        "cue r=(%.4f,%.4f) v=(%.4f,%.4f)  "
                        "obj r=(%.4f,%.4f) v=(%.4f,%.4f)\n",
                        t, evn(e.type), e.i, e.j, e.rail,
                        bs[0].r.x, bs[0].r.z, bs[0].v.x, bs[0].v.z,
                        bs[1].r.x, bs[1].r.z, bs[1].v.x, bs[1].v.z);
        });
        std::printf("final cue x=%.4f  obj x=%.4f\n",
                    w.balls[0].r.x, w.balls[1].r.x);
    }
}
