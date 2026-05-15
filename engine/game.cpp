#include "engine/game.h"

#include <algorithm>
#include <limits>

namespace cue {
namespace {

int cueIndex(const std::vector<Ball>& b) {
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i].type == BallType::Cue) return static_cast<int>(i);
    return -1;
}

}  // namespace

int legalTarget(const std::vector<Ball>& balls) {
    int best = std::numeric_limits<int>::max();
    for (const Ball& b : balls)
        if (b.type != BallType::Cue && !b.pocketed) best = std::min(best, b.id);
    return best == std::numeric_limits<int>::max() ? -1 : best;
}

ShotOutcome simulateShot(World& w) {
    ShotOutcome out;
    const int ci = cueIndex(w.balls);
    const int cueId = ci >= 0 ? w.balls[ci].id : -1;
    const int legalBefore = legalTarget(w.balls);

    int firstContact = -1;
    w.simulate([&](double, const WorldEvent& e, const std::vector<Ball>& bs) {
        if (e.type == EventType::BallBall && firstContact == -1) {
            const Ball& A = bs[e.i];
            const Ball& B = bs[e.j];
            if (A.type == BallType::Cue && B.type != BallType::Cue)
                firstContact = B.id;
            else if (B.type == BallType::Cue && A.type != BallType::Cue)
                firstContact = A.id;
        }
    });

    out.firstContact = firstContact;
    for (const Ball& b : w.balls) {
        if (!b.pocketed) continue;
        if (b.id == cueId) out.cueScratched = true;
        else out.pocketed.push_back(b.id);
    }

    if (out.cueScratched)            out.foul = Foul::Scratch;
    else if (firstContact == -1)     out.foul = Foul::NoContact;
    else if (firstContact != legalBefore) out.foul = Foul::WrongBallFirst;

    const bool ninePotted =
        std::find(out.pocketed.begin(), out.pocketed.end(), 9) !=
        out.pocketed.end();
    out.won = ninePotted && out.foul == Foul::None;
    return out;
}

}  // namespace cue
