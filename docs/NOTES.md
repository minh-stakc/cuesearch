# Engineering judgment log

The interview-relevant part of this project is not any single physics gate
- it is the judgment calls. Each is a real decision, in git history.

- **Re-derived a wrong published formula.** The transcribed cue-strike spin
  `−c·F·sinθ + b·F·cosθ` *cancels* for any pure follow/draw offset (zero
  spin - physically impossible). Discarded it, derived `ω₀ = (P×J)/I` from
  torque = r×J. A physics-sanity test caught it. - `f94c238`
- **Diagnosed Wilkinson ill-conditioning** in the quartic solver instead of
  loosening the test. Added Newton polish + root **deflation** for coverage,
  and a conditioning-free **residual oracle** (`|P(x)|≈0`) - a *stronger*
  correctness statement than root-distance. - `1bdd2d1`
- **Root-caused a tunnel-through bug**: the raised cushion contact injected a
  vertical velocity the planar (non-jump) model can't carry, corrupting
  classification. Fix = the slate absorbs the vertical impulse (correct for
  the non-jump regime; jumps are the documented roadmap). - `8ae80ef`
- **Refused to fudge.** Trisect's exact 3×cut coefficient needs Dr. Dave's
  good-action calibration I didn't have - validated the unambiguous
  relational physics (follow < stun < draw) and *documented the deferral*
  rather than tuning constants to a number. - `4198b94`
- **Distinguished calibration from fudging.** Cushion COR *is* an empirical
  constant whose purpose is fitting measured efficiency - tuning it to
  Dr. Dave's ~0.5–0.6 is correct; tuning trisect to fake a result is not. - `8ae80ef`
- **Split the 90° rule test** into the textbook `e=1` law and the exact
  physical `e=0.95` deviation `atan(tanφ·2/(1−e))` - predicted analytically,
  not "close enough." - `d7a6165`
- **Owned test-setup bugs** (a "no-contact" cue aimed into a side pocket; a
  CP2 head-on expecting placeholder physics) as setup errors, not engine
  bugs - proven analytically each time.
- **Dropped a pitch claim honestly.** pooltool won't install on Windows →
  removed the differential-test line rather than paper over it; validation
  stands on measured data + invariants (the stronger basis). - `8ae80ef`
- **Changed the objective, not the heuristics - twice more.** Kicks/banks
  were added as *candidates*; the EV objective ranks them (direct preferred
  when clean, kick only when forced - regression-tested). Then the objective
  itself moved from pot-EV to **win-EV** (2-ply self-play, 3-consecutive-foul
  terminal, flat ball-in-hand bonus): **safeties emerge** - on the snooker
  layout the planner switches from a 12% kick (pot-EV) to *play safe*,
  P(win)=0.75 (win-EV). Same input, see `docs/example_snooker.out`. Safety
  was never special-cased; the math reconstructs the strategy. Bounded by
  design: opponent = self-play, 2-ply cap, ball-in-hand = flat +0.25 (not a
  placement search), 2-foul-warning ignored, position value = best-direct
  makeability proxy. **These are documented simplifications, deliberately
  shipped - not silent gaps**; the win-EV *structure* is the contribution.
- **Caught a per-rollout recursion blow-up** in the win-EV (planRunout was
  invoked inside every rollout -> exponential). Restructured to tally
  outcome fractions and evaluate continuation/opponent ONCE per state. A
  performance bug, root-caused and fixed, not worked around.
- **Caught (via a user-reported symptom) a degeneracy in the bounded
  win-EV and routed around it honestly.** On multi-ball racks the win-EV
  refused obvious gimmes and always played safe. Root cause: a pot is
  valued by *my* continuation = the cheap one-ball makeability proxy
  (pessimistic, models no cue control), while a safety is valued as
  `1 - (opponent's equally-pessimistic value)` ≈ high -> safety
  structurally dominates. This is the documented proxy simplification
  biting harder than expected; the win-EV is only sound for the
  *snookered* case it was built for. Fix: `solve` now runs the validated
  pot-EV positional planner for offence and falls back to win-EV/safety
  ONLY when no makeable shot exists. Diagnosed with a controlled layout,
  not hand-waved; the limitation is documented, not hidden.
- **Two real match bugs found from a user symptom ("we keep scratching/
  safing on the 1"), fixed; the residual honestly attributed.** (1) On a
  SCRATCH the cue is flagged `pocketed`; ball-in-hand never cleared it, so
  every subsequent shot had a phantom (off-table) cue and fouled forever --
  an endless safe/scratch spiral. Fixed: ball-in-hand un-pockets the cue.
  (2) The break was a dead-full, no-spin hit that never pocketed and always
  left a cluttered table; replaced with a firm, slightly-cut strong-amateur
  break that realistically pots / goes dry / scratches. (3) The remaining
  behaviour -- pots when handed a good position (ball-in-hand) but bails to
  safety once it must SET UP the next ball -- is the documented bounded-
  solver ceiling (no cue-control / positional planning), NOT a bug. It can
  make balls; it cannot run out. A true positional solver is a separate,
  much larger build, deliberately out of scope.
- **Combo / carom** (instant 9-ball win) added as candidates, gated on the
  9 sitting within ~0.3 m of a pocket so they cost nothing in normal
  layouts. Validated: the combo geometry is a real, legal instant win
  (lowest ball hit first, 9 down, no foul) and the win-EV finds it
  (P(win)=1.0). Honest cosmetic limitation: when several shots tie at
  P(win)=1.0 (a dead-on-the-9 rack), the displayed *label* is whichever
  ranked first (may read "SAFE" though the executed effect is the combo) --
  the decision and outcome are correct; the label is not tie-broken. Flagged,
  not polished away (scope discipline: stop adding).

- **Pivoted the whole framing on evidence.** Verified-the-gap research
  showed `pooltool` already does fast event-based simulation, so the novelty
  was reframed from "faster sim" (indefensible) to the **multi-shot solver +
  validation discipline** (genuine) - *before* writing code.
- **Pre-committed a measurable bar for the hardest feature, then honoured
  it when it failed.** Asked to build a "true run-out solver", wrote
  docs/POSITIONAL_DESIGN.md FIRST with success (>=60% run-out) and failure
  (<25% -> abandon) gates, plus a mandatory engine-leave calibration
  (CP-pre) before building on the engine recursively. CP-pre passed; POS-a
  (the shape-aware shot-shaper) passed -- it demonstrably optimises the
  leave vs a shape-blind planner (0.227 > 0.198). POS-b's run-out gate
  measured **0/100** (avg 0.62 balls/run): under stroke noise the bounded
  shaper can't even reliably pot the first ball. Below the 25% failure
  threshold -> STOPPED, shipped the shape planner OPT-IN only (never on a
  default path), documented the honest boundary. Did NOT move the
  goalposts or "fix-and-re-measure" past the pre-committed stop. Setting a
  falsifiable bar in advance and respecting it is the point.
- **User-supplied skill-curve calibration anchor, applied honestly.**
  Centralised execution noise into ONE constant (k::AIM_SIGMA /
  SPEED_SIGMA) used by solver/winsolve/plan -- a single calibratable
  knob. Validated the model against the user's anchor (good player:
  straight shot ~10/10, harder shots lower): with sigma=0.009 the
  measured curve via CP7's *proven* geometry is pPot 1.00 (straight) ->
  0.95 -> 0.88 -> 0.82 (steeper), monotone -- straight matches 1.0
  exactly. Two bespoke calibration harnesses were discarded after they
  surfaced their own geometry bugs (cue off-table; a straight-shot
  scratch confound) -- reused CP7's validated layout instead rather than
  trust a hand-rolled rig. Did NOT force-fit sigma to "0.60 at 90 deg":
  a literal 90 deg cut is geometrically unpottable (P->0 under any aim
  model), so that exact endpoint is ill-posed and is documented, not
  faked. The skill-curve SHAPE is now a regression gate.
