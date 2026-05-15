#pragma once
// Mathavan et al. (2014) frictional ball-ball collision, resolved by
// integrating Coulomb friction over the accumulated normal impulse.
// Throw (cut- and spin-induced) EMERGES from the tangential impulse — it is
// never hardcoded. INTERNALIZE: line-of-centres frame, the slip recurrence,
// why total normal impulse = (1+e)*m_reduced*v_closing.
#include "engine/ball.h"

namespace cue {

// Resolve an in-contact pair (|rB-rA| ~ 2R). Mutates v,w of both.
// muOverride >= 0 forces a constant ball-ball friction (used to verify the
// frictionless 90-degree limit); < 0 uses the speed-dependent Marlow fit.
// eOverride >= 0 forces the restitution (e=1 -> textbook 90-degree rule);
// < 0 uses the physical k::E_BALL.
void resolveBallBall(Ball& A, Ball& B, double muOverride = -1.0,
                     double eOverride = -1.0);

}  // namespace cue
