# Engineering judgment log

The interview-relevant part of this project is not any single physics gate
— it is the judgment calls. Each is a real decision, in git history.

- **Re-derived a wrong published formula.** The transcribed cue-strike spin
  `−c·F·sinθ + b·F·cosθ` *cancels* for any pure follow/draw offset (zero
  spin — physically impossible). Discarded it, derived `ω₀ = (P×J)/I` from
  torque = r×J. A physics-sanity test caught it. — `f94c238`
- **Diagnosed Wilkinson ill-conditioning** in the quartic solver instead of
  loosening the test. Added Newton polish + root **deflation** for coverage,
  and a conditioning-free **residual oracle** (`|P(x)|≈0`) — a *stronger*
  correctness statement than root-distance. — `1bdd2d1`
- **Root-caused a tunnel-through bug**: the raised cushion contact injected a
  vertical velocity the planar (non-jump) model can't carry, corrupting
  classification. Fix = the slate absorbs the vertical impulse (correct for
  the non-jump regime; jumps are the documented roadmap). — `8ae80ef`
- **Refused to fudge.** Trisect's exact 3×cut coefficient needs Dr. Dave's
  good-action calibration I didn't have — validated the unambiguous
  relational physics (follow < stun < draw) and *documented the deferral*
  rather than tuning constants to a number. — `4198b94`
- **Distinguished calibration from fudging.** Cushion COR *is* an empirical
  constant whose purpose is fitting measured efficiency — tuning it to
  Dr. Dave's ~0.5–0.6 is correct; tuning trisect to fake a result is not. — `8ae80ef`
- **Split the 90° rule test** into the textbook `e=1` law and the exact
  physical `e=0.95` deviation `atan(tanφ·2/(1−e))` — predicted analytically,
  not "close enough." — `d7a6165`
- **Owned test-setup bugs** (a "no-contact" cue aimed into a side pocket; a
  CP2 head-on expecting placeholder physics) as setup errors, not engine
  bugs — proven analytically each time.
- **Dropped a pitch claim honestly.** pooltool won't install on Windows →
  removed the differential-test line rather than paper over it; validation
  stands on measured data + invariants (the stronger basis). — `8ae80ef`
- **Pivoted the whole framing on evidence.** Verified-the-gap research
  showed `pooltool` already does fast event-based simulation, so the novelty
  was reframed from "faster sim" (indefensible) to the **multi-shot solver +
  validation discipline** (genuine) — *before* writing code.
