# Break-and-run (BR) - pre-registration (commit BEFORE any BR code)

This document is committed as a standalone commit BEFORE any BR-1..BR-3
code lands. That commit order IS the audit trail (same discipline that
gave RO-1..RO-4 its integrity). If BR code appears in git history before
this doc, the pre-registration is void and the result must NOT be
claimed.

## Why this is a NEW effort, not a rescue of RO-4

RO-4 (the ball-in-hand run-out gate) FAILED its pre-committed bar and was
STOPPED, shipped opt-in, documented. That stop stands and is NOT reopened.

The RO-4 diagnosis explicitly NAMED the missing machinery as out of scope
*at that time*: per-candidate Monte-Carlo-over-noise (not a single
noiseless difficulty-table lookup), a finer difficulty model, and
deeper/wider search. Building that named machinery, as a separately
pre-registered effort against a new task, is legitimate R&D. It is NOT
goalpost-moving, because:

- It does not lower or reopen RO-4's 35/15 bands.
- The task is genuinely different and harder: full **rack -> break ->
  run-out** (a stochastic opening break, then clear from whatever it
  leaves), not ball-in-hand on a clean rack.
- The bar below is set BEFORE building, from external reference data.

The line that must NOT be crossed (the p-hacking line): every change must
be a NAMED ARCHITECTURAL PIECE the RO-4 diagnosis already flagged. Tuning
the noise sigma, the rack, the trial count, or fishing RNG seeds until
15% appears is FORBIDDEN and voids the result.

## The bar (single threshold, set from external data)

**SUCCESS = break-and-run rate >= 15%.** Anything below is documented
honestly where it plateaus, and we STOP.

Calibration (not chosen by us; from Dr. Dave's compiled tournament
statistics, drdavepoolinfo.com/faq/break/stats/, and AzBilliards /
Accu-Stats summaries):

- Pro 9-ball tournament break-and-run is generally **15-43%**
  (2022 World Pool Championship: 43%; historic top pros e.g. Strickland
  / Archer ~30-33%; "good amateur" ~25%).
- **15% is the LOW END of professional tournament play** -> an honest,
  conservative bar for "a good player," as the user specified.
- Our player's execution noise is already calibrated and frozen
  (`k::AIM_SIGMA = 0.009`, validated to "good player: straight shot
  ~10/10, harder shots lower"). The noise model is NOT a tunable knob
  for this effort.

Single threshold only. No multi-band 35/15 gate (that was RO-4's frame
and is settled). >=15% = success; <15% = documented ceiling + stop.

## Named architectural changes (the only legitimate levers)

1. **BR-1: per-candidate Monte-Carlo-over-noise.** Score each candidate
   shot by the MC mean of its real outcome over N execution-noise
   samples, NOT one noiseless difficulty-table lookup. This is the single
   biggest missing piece per Smith (PickPocket) / CueCard. Unit-tested in
   isolation FIRST: does it select robustly different shots than the
   noiseless lookup, and reduce the variance of the selected shot's pPot
   under noise? If the architectural change does NOT change shot
   selection / robustness, that null result is itself falsifiable ->
   document and STOP (do not escalate effort to force a difference).

2. **BR-2: realistic break model + spread-plausibility validation.** A
   9-ball break is far outside the engine's validated energy regime
   (break cue speed ~9-14 m/s vs the ~3 m/s of validated CP shots),
   with many near-simultaneous contacts and rack jitter dominant. Build
   the break (cue near side rail, hit the apex/1 slightly off-centre with
   follow, rack with positional jitter) and VALIDATE the resulting
   distribution against reference BEFORE counting any break-and-run:
   - legal-break rate (>=1 ball pocketed or driven to a rail, no scratch)
     in the tournament-plausible band (~60-90%);
   - mean balls pocketed on the break order ~0-2 (occasionally more);
   - balls genuinely scattered (not clumped), 1-ball reachable.
   If the break model can't produce a plausible spread distribution, that
   is a BR-2 failure to surface honestly - a break-model artifact must
   NOT be allowed to masquerade as solver progress downstream.

3. **BR-3: finer difficulty table / deeper-wider goal-directed search.**
   Higher-resolution difficulty model and/or deeper/wider beam - only the
   named pieces from the RO-4 diagnosis, NOT noise/rack/sample tuning.

## Compute budget (part of the pre-registration)

RO-4 was ~141 s for 24 trials. BR adds per-candidate MC + a break + a
finer table and will be much heavier.

- **Final BR gate: <= 30 minutes wall-clock for the gate run.**
- Sample may be reduced for compute and MUST be stated; the planner
  config and noise MUST NOT be weakened to fit the budget.
- The budget is fixed here so "let the run get heavier" cannot become a
  hidden form of effort-iteration.

## Stop condition (explicit, binding)

The final BR gate is run **once**, within the compute budget, with the
frozen noise model. If the measured break-and-run rate is **< 15%**:

- Document the measured number and *where* it plateaus (which stage -
  break legality, first-ball pot, run-chain length).
- STOP. No parameter iteration, no seed fishing, no "one more run."
- Ship `planRunOut` + BR machinery OPT-IN, like RO.

If **>= 15%**: that is a genuine capability result (good-player level,
the literature's low-pro band), reported with the full audit trail
(this doc's commit hash predating all BR code).

A null/negative result here is a valid, publishable outcome and is worth
more to the profile than a p-hacked 15%.
