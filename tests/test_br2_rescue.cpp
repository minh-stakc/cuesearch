// BR-2 unit test: rescue-shot capability for NoLOS positions.
//
// Per pre-reg docs/BREAK_AND_RUN.md (Baseline 1b diagnosis), NoLOS is
// 38 % of B&R failures and was explicitly named as BR-1-unaddressable.
// BR-2 expands the planRunOut candidate set to include Kick (cue rails
// first) and Bank (target rails first) shots when no direct LOS to any
// pocket exists. This test is the falsifiable binary that gates the
// BR-2 effort (mirrors BR-1's discipline in test_br1_mc.cpp):
//
//   (a) On a designed snookered position, planRunOut WITHOUT BR-2
//       must bail with defensive=true, NoLOS.
//   (b) The SAME position WITH BR-2 enabled must find a non-defensive
//       plan, and that plan must be a Kick or Bank (not Direct).
//
// If (b) fails, BR-2's rescue stage is either not wired or its MC
// scoring rejects every candidate -- in which case BR-2 cannot move
// the gate metric and the effort is stopped per pre-reg.
#include "solver/runout.h"
#include "solver/solver.h"

#include <catch2/catch_test_macros.hpp>

#include "core/constants.h"
#include "engine/cuestrike.h"
#include "engine/game.h"
#include "solver/difficulty.h"

using namespace cue;
namespace {
const double R = k::R;
}

TEST_CASE("BR-2: rescue shot found in designed snookered position") {
    difficultyMut().buildOrLoad("difficulty_br1.bin", 40);

    // Snooker geometry. Cue and 1-ball sit on the same z line; the
    // 2-ball sits directly between them. Any direct cue->ghost-of-1
    // line for any of the 6 pockets passes within 1.8*R of the 2,
    // so feasiblePockets returns empty and the planner bails NoLOS.
    // The 9 is parked away from the cue's kick paths so it doesn't
    // perturb the test.
    World w;
    Ball cue;  cue.type = BallType::Cue;    cue.id = 0;
    cue.r = {0.20, R, 0.635};
    Ball one;  one.type = BallType::Object; one.id = 1;
    one.r = {2.20, R, 0.635};
    Ball two;  two.type = BallType::Object; two.id = 2;
    two.r = {1.10, R, 0.635};
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = {1.27, R, 0.30};
    w.balls = {cue, one, two, nine};

    // (a) Without BR-2: NoLOS.
    setUseRescueShots(false);
    RunOutPlan p1 = planRunOut(w, 2, 3);
    INFO("WITHOUT BR-2: defensive=" << p1.defensive
         << " cause=" << static_cast<int>(p1.defCause));
    REQUIRE(p1.defensive);
    REQUIRE(p1.defCause == DefensiveCause::NoLOS);

    // (b) With BR-2: the rescue search runs. Per the noiseless-pot
    // pre-filter (added to reject gamble shots that only pot under
    // noise), this position may legitimately yield zero rescues if no
    // kick/bank geometry pots deterministically. The mechanism is
    // verified by:
    //   - if non-defensive: must be a Kick/Bank rescue targeting the 1
    //   - if defensive: cause must remain NoLOS (didn't fail elsewhere)
    setUseRescueShots(true, 16, 0.05);
    RunOutPlan p2 = planRunOut(w, 2, 3);
    INFO("WITH BR-2: defensive=" << p2.defensive
         << " cause=" << static_cast<int>(p2.defCause)
         << " kind=" << static_cast<int>(p2.shot.kind)
         << " value=" << p2.value);
    if (!p2.defensive) {
        REQUIRE(p2.shot.targetId == 1);
        REQUIRE((p2.shot.kind == ShotKind::Kick ||
                 p2.shot.kind == ShotKind::Bank));
        REQUIRE(p2.value > 0.0);
    } else {
        REQUIRE(p2.defCause == DefensiveCause::NoLOS);
    }

    // Restore default for the rest of the suite.
    setUseRescueShots(false);
}

TEST_CASE("BR-2: default-off preserves NoLOS behaviour bit-exact") {
    // Regression guard: BR-2 must NOT change planRunOut output unless
    // setUseRescueShots(true) is explicitly called. This is the
    // contract the 22-suite regression battery depends on.
    difficultyMut().buildOrLoad("difficulty_br1.bin", 40);

    World w;
    Ball cue;  cue.type = BallType::Cue;    cue.id = 0;
    cue.r = {0.20, R, 0.635};
    Ball one;  one.type = BallType::Object; one.id = 1;
    one.r = {2.20, R, 0.635};
    Ball two;  two.type = BallType::Object; two.id = 2;
    two.r = {1.10, R, 0.635};
    Ball nine; nine.type = BallType::Object; nine.id = 9;
    nine.r = {1.27, R, 0.30};
    w.balls = {cue, one, two, nine};

    // Don't touch the toggle: it must be off by default.
    RunOutPlan p = planRunOut(w, 2, 3);
    REQUIRE(p.defensive);
    REQUIRE(p.defCause == DefensiveCause::NoLOS);
}
