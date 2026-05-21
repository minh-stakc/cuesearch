// Step a ball through the motion model from a chosen initial state.
#include <cstdio>
#include "core/constants.h"
#include "core/frame.h"
#include "engine/motion.h"
using namespace cue;

int main(int argc, char** argv) {
    Ball b;
    b.type = BallType::Cue; b.id = 0;
    b.r = {1.04286, k::R, 0.63500};
    b.v = {0.0753, 0.0, 0.0};            // post-STUN cue v (my-fix value)
    b.w = {0.0, 0.0, -14.664};           // post-STUN cue w
    ClothParams cloth;
    Segment s = beginSegment(b, cloth);
    std::printf("seg state=%d T=%.4fs v=(%.4f,%.4f,%.4f) w=(%.4f,%.4f,%.4f)\n",
                (int)s.state, s.T, b.v.x, b.v.y, b.v.z, b.w.x, b.w.y, b.w.z);
    for (double t = 0.0; t <= 1.5; t += 0.05) {
        Ball at = s.at(std::min(t, s.T));
        if (t > s.T) {
            // transitioned, advance to next segment(s)
            b = s.endBall();
            double tRemain = t - s.T;
            while (b.state != BallState::Stationary && tRemain > 1e-9) {
                Segment ns = beginSegment(b, cloth);
                if (tRemain < ns.T) {
                    at = ns.at(tRemain);
                    tRemain = -1; break;
                }
                tRemain -= ns.T;
                b = ns.endBall();
                at = b;
            }
            std::printf("t=%.3f r=(%.4f,%.5f,%.4f) v=(%.4f,%.5f,%.4f) "
                        "state=%d (post-Sliding)\n",
                        t, at.r.x, at.r.y, at.r.z, at.v.x, at.v.y, at.v.z,
                        (int)at.state);
            // re-run from b's segment
            if (b.state == BallState::Stationary) break;
            s = beginSegment(b, cloth);
        } else {
            std::printf("t=%.3f r=(%.4f,%.5f,%.4f) v=(%.4f,%.5f,%.4f) "
                        "state=%d\n",
                        t, at.r.x, at.r.y, at.r.z, at.v.x, at.v.y, at.v.z,
                        (int)s.state);
        }
    }
}
