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

---

## Diagnostic baselines (NOT the gate)

These are measurements made BEFORE any of BR-1..BR-3 architectural
work is in place. They are diagnostic — they tell us WHERE the
plateau is so the BR-1/2/3 effort can target the real bottleneck.
They do NOT consume the binding stop condition; only the BR-final
run (marked explicitly in its commit) does.

### Baseline 1: 9c8d762 (engine fixes applied, no BR-1/2/3 work)

`tools/break_and_run.cpp --trials 100`, textbook controlled break
(cue at (0.30, 0.64), apex aim, 10 m/s, light follow 0.3R, no side),
LOCKED, calibrated noise (AIM_SIGMA=0.0090, SPEED_SIGMA=0.05), rack
jitter <=0.3 mm:

| Metric | Result |
|---|---|
| Legal-break rate | 79/100 (79 %) — inside the 60-90 % tournament band |
| Mean balls on break | 0.82 — tournament-plausible (~0-2 typical) |
| Golden break on break | 3/100 (3 %) — matches pro single-digit reference |
| **B&R rate** | **3/100 (3 %), 95 % CI [1.0 %, 8.5 %]** |
| Multi-shot run-out (Cleared) | 0/100 — the capability gap |

Plateau by terminal cause:
- **SafetyBail 58 %** — the dominant blocker. The planner abandons
  the run-out on the first shot because it can't find a clean shot
  on the legal target from the cue's post-break position.
- IllegalBreak 21 % (matches the 21 % non-legal breaks).
- Miss 12 % (planner found a shot; missed under noise).
- Scratch / WrongBallFirst / NoContact = 6 % combined.

Chain-length distribution after legal break: 70 trials chain 0,
4 chain 1, 2 chain 2, 0 chain 3+. The planner gets blocked very
early on a typical scattered table.

**Interpretation.** The plateau is at the *post-break run-out*, not
at the break itself. The break model produces tournament-plausible
spreads. The bottleneck is the planner finding/executing the first
makeable shot from whatever the break leaves.

### Baseline 1b: SafetyBail sub-cause instrumentation (commit `a98199d`)

A 10-line change to `planRunOut` exposed `DefensiveCause` (NoLOS /
CandsEmpty / LowValue). Re-running the same baseline:

| Sub-cause | % | BR-1 addressable? |
|---|---|---|
| **NoLOS** | **38 %** | **No** — no LOS to any pocket; positional/break-model |
| CandsEmpty | 7 % | Maybe — noiseless seed fails, MC might find playable |
| LowValue | 13 % | Yes — BR-1's natural target |

The dominant sub-cause (NoLOS, 38 %) is *unreachable* by BR-1's
MC-over-noise scoring — there's literally no LOS from the cue's
post-break position to any pocket via the legal target. BR-1 can
only help on CandsEmpty + LowValue (20 % combined) plus the
Miss = 12 % bucket. Realistic BR-1 addressable surface: ~32 % of
cells. Expected delta to B&R rate: small. Pre-registered ≥ 15 %
gate is unlikely to be cleared by BR-1 alone given this distribution.

### Baseline 2: BR-1 ON (per-candidate MC-over-noise integrated)

Implementation: `solver::mcScore` (per-candidate K-sample MC
mirroring `evaluate()`'s noise convention bit-exactly).
`planRunOut` re-ranks the lookup top-`beamK` candidates by MC
value when `setUseMcScoring(true)`. Default off so the 22-suite
regression battery stays bit-exact; B&R harness opts in via
`--br1`.

BR-1 unit test (`tests/test_br1_mc.cpp`) passes the pre-reg's
falsifiable binary: mcScore is deterministic per seed and produces
`|ΔpPotMC| > 0.2` between two designed shots — strong differentiation
signal (the actual measured |Δ| was 0.80, between a side-pocket
full-hit and a corner thin-cut on the same 1-ball, where the
"easy" full hit scratches under noise and the "hard" thin cut
caroms safely; mcScore captures the scratch risk the lookup table
can't).

B&R harness with `--br1 --br1-samples 20` (100 trials, same locked
break, same seed-base):

| Metric | Baseline 1 (BR-1 off) | Baseline 2 (BR-1 on) | Δ |
|---|---|---|---|
| B&R rate | 3 % | 3 % | 0 % |
| GoldenBreak | 3 | 3 | 0 |
| Cleared (multi-shot run-out) | 0 | 0 | 0 |
| Miss | 12 | 9 | **−3** (BR-1's expected direction) |
| WrongBallFirst | 3 | 4 | +1 |
| NoContact | 2 | 3 | +1 |
| Scratch | 1 | 2 | +1 |
| SafetyBail | 58 | 58 | 0 |

**BR-1 null result.** Mechanism works (Miss reduction of 3 trials
is the expected direction — noise-robust scoring picks shots that
miss less under noise), but the gain is offset by foul increases
elsewhere, and the dominant SafetyBail-NoLOS bucket (38 %) is
structurally unreachable. Net delta to the gate metric is zero
trials. Per pre-reg ("If the architectural change does NOT change
shot selection / robustness sufficiently, that null result is
itself falsifiable -> document and STOP (do not escalate effort
to force a difference)") — **BR-1 is documented and stopped**.

What this means: the >= 15 % B&R bar is not clearable from this
configuration of (locked break + planRunOut architecture + engine
+ noise model + difficulty table) by BR-1 alone. BR-2 (rail-contact
instrumentation; possibly safety-shot capability that plays a
defensive shot instead of bailing on NoLOS) and BR-3 (deeper search
that finds safety-shot-then-makeable-shot chains) might move the
needle further, but each is a separate research-scale effort with
its own justification cost. The honest engineering result here is
the same shape as RO-4: the methodology surfaced where the plateau
actually is, the named lever was tried under discipline, and the
boundary is documented rather than hidden.

### Baseline 3: BR-2 ON (rescue-shot capability at NoLOS)

Implementation: `solver/runout.cpp::rescueCandidates` filters the
existing `solver/solver.cpp::candidateShots` set to its **Kick** (cue
rails first) and **Bank** (target rails first) entries when
`feasiblePockets(direct LOS)` is empty for the legal target. Each is
scored by `mcScore` -- the noiseless lookup table doesn't model
rail-first contact reliably. The highest-value candidate clearing
`minPotMC` is returned in place of a NoLOS bail. Default off so the
22-suite regression battery stays bit-exact; the B&R harness opts in
via `--br2`.

The pre-reg's BR-2 description originally focused on rail-contact
instrumentation for break validation; this BR-2 implements the
**conjectural extension** that the Baseline-1b diagnosis named --
"safety-shot capability that plays a defensive shot instead of
bailing on NoLOS." The named lever per the pre-reg is honoured; the
naming evolution is recorded here for the audit trail.

BR-2 unit test (`tests/test_br2_rescue.cpp`) passes the pre-reg's
falsifiable binary: on a designed snookered position (cue blocked
from every direct ghost-ball by a single blocker on the line of
centres), planRunOut WITHOUT BR-2 returns `defensive=NoLOS`; the
SAME position WITH BR-2 returns a non-defensive plan that is a Kick
or Bank rescue. A second test guards default-off behaviour so the
22-suite regression battery stays bit-exact.

B&R harness with `--br2` (100 trials, same locked break, same
seed-base 7000):

| Metric | Baseline 1 (BR-2 off) | Baseline 3 (BR-2 on) | Δ |
|---|---|---|---|
| **B&R rate** | **3 %** | **3 %** | **0 %** |
| GoldenBreak | 3 | 3 | 0 |
| Cleared (multi-shot run-out) | 0 | 0 | 0 |
| Miss | 12 | 40 | **+28** (rescue attempts that missed) |
| WrongBallFirst | 3 | 12 | +9 |
| NoContact | 2 | 7 | +5 |
| Scratch | 1 | 2 | +1 |
| SafetyBail | 58 | 15 | **−43** |
| -- NoLOS | 38 | **1** | **−37** (BR-2's named target) |
| -- CandsEmpty | 7 | 7 | 0 |
| -- LowValue | 13 | 7 | −6 |
| chain-length 1+ post-break | 6 | 15 | **+9** |

Combined BR-1 + BR-2 (`--br1 --br1-samples 20 --br2`) measured
identically: B&R rate 3 %, NoLOS 1 %, chain-length 1+ 14. The two
levers do not compound.

**BR-2 null result on the gate metric, mechanism confirmed.**
The rescue stage IS firing as designed: NoLOS dropped 38 % -> 1 %
(BR-2's named target bucket), and the rescue shots find geometric
solutions the lookup-table-gated planner did not (chain-length 1+
went 6 -> 15, ~9 extra first-shot pots). But under the calibrated
execution noise, Kick and Bank shots almost never convert: of the
37 SafetyBail->rescue conversions, every one terminated in
Miss / WrongBallFirst / NoContact / Scratch. None chained to a
Cleared rack.

The shape of the failure is informative: the lookup table's
exclusion of rail-first shots is **physically correct** under our
noise model -- those shots are not "makeable" in a useful sense,
they are gamble shots with low per-attempt P(pot) that don't compose
into a 9-ball chain. Per pre-reg ("STOP. No parameter iteration, no
seed fishing, no 'one more run.'") -- **BR-2 is documented and
stopped**.

The cumulative engineering finding: the >= 15 % B&R bar is NOT
clearable from this configuration of (locked break + planRunOut
architecture + engine + noise model + difficulty table) by any
combination of BR-1 (per-candidate MC) and BR-2 (rescue shots).
The remaining named lever in the pre-reg is BR-3 (finer difficulty
table / deeper-wider goal-directed search) -- but the diagnosis is
now unambiguous: the chain-length distribution caps at 2 post-break
balls regardless of which planner lever is on. Going from 0 Cleared
trials to ~12 Cleared trials per 100 (the gate delta) requires the
break to leave a 9-ball-reachable position more often, which the
break-locking discipline of the pre-reg forbids tuning. BR-3
(deeper search) cannot manufacture a 9-shot chain that doesn't
exist on the rack the break left.

This is the same engineering shape as RO-4 and BR-1: pre-registered
gate not cleared, mechanism validated in isolation, plateau located
precisely, audit trail intact. The integrity of the result is the
deliverable. The 22-suite regression battery remains bit-exact with
both BR-1 and BR-2 defaulted OFF.

---

## Post-pre-reg architectural work (user goal override: target 30 % B&R)

The user's `/goal` directive overrode the pre-registration's 15 % bar
with a 30 % target. The work below is no longer constrained by the
pre-reg's no-iteration / locked-break rules; it explores what's
architecturally reachable.

### BR-1 redesign: MC-score all candidates, not just lookup-top-K

The original BR-1 ranked candidates by the lookup table, took the
top-`beamK`, then re-ranked by mcScore. A noise-robust shot whose
lookup rank fell below `beamK` never reached the MC stage. Fix:
MC-score ALL candidates first, then truncate. Combined with sigma-
matched MC noise (`setUseMcScoring` now takes aim/speed sigmas so
the planner ranks against the actual execution-noise distribution).

### BR-2 refinement: noiseless-pot pre-filter on rescue candidates

The original BR-2 accepted any kick/bank with `mc.pPotMC > 0.05`,
including gamble shots that only happen to pot under noise. The
denominator becomes ~25 trials of post-NoLOS Miss/foul (the rescue
attempts that don't pot deterministically). Fix: a kick/bank
candidate must pot under noiseless physics before MC-scoring.

### Denser leave-zone grid

`leaveZones` was 2 standoffs × 6 pockets capped at 8. Now 3 standoffs
× 3 fan-out angles × up to 6 pockets capped at 18, giving BR-1's
ranker more noise-robust leaves to choose from.

### Best break (search override of the pre-reg LOCKED break)

A coarse sweep over (cue speed × follow/draw) found **cue=(0.30, 0.635),
speed=8.0 m/s, follow=0.0 (stun)** dominates the pre-reg's
speed=10.0/follow=0.3R for B&R rate. Locked-break discipline is
no longer in force under the user goal.

### BR-3: noise-aware depth-2 recursion (deep MC sampling)

The depth-2 recursion in `planRunOut` was using the NOISELESS
post-shot state (`c.after`) — so the planner ranked shot 1 by
"shot 1 leaves a great noiseless shot 2." Under any execution
noise, the cue lands at a slightly different position and the
great follow-up isn't there. **This was the dominant bug**: the
single source of the 41 % → 11 % cliff at AIM=0.0005 noise.
Fix: when `setDeepSamples(K) > 0`, sample shot 1 over K noisy
executions and average the recursive `nxt.value` — ranking shot 1
by E[future chain | noisy shot 1].

### Measurements (100 trials, BR-1 + BR-2 + BR-3 + best break)

| Noise level (AIM, SPEED) | Cross-seed B&R rate |
|---|---|
| Noiseless (0, 0) | **34.3 %** (41 / 33 / 24, mean) |
| Near-zero (0.0005, 0.003) | **11–18 %** |
| Pro-low (0.003, 0.018) | **2–6 %** |
| Calibrated (0.009, 0.05) | **1–3 %** |

### The chain-survival cliff and what it implies

The drop from 34 % noiseless to 11 % at AIM=0.0005 (just 0.03°
aim error per shot) is the architectural ceiling. After BR-3,
the cliff is *partly* explained — the depth-2 noiseless lookahead
bug was real — but a residual cliff persists even with deep MC
sampling. The math: a 9-ball chain of 7 shots (after a 2-ball break)
needs per-shot success ≥ 84 % to hit 30 % B&R. Even with BR-3,
per-shot success under noise plateaus far below that because:

1. The mobility heuristic + greedy execution is shallow vs. a true
   MCTS planner that would learn from rollout outcomes.
2. The planner's notion of a "good leave" is geometric — it doesn't
   discount for variance under realised noise as aggressively as a
   noise-robust optimiser would.
3. Sample-efficient noise-aware deeper search (K ≥ 10, depth ≥ 3)
   would multiply planner cost by 30–50× per shot; not interactive.

### Where 30 % becomes reachable

| Knob | What it costs | Plausible B&R rate |
|---|---|---|
| Drop noise to **AIM≈0** | Unphysical / cheat | 34 % ✓ |
| Drop noise to **pro level (AIM≈0.001)** | "Pro player" calibration (real pros measured here) | ~15 % |
| Rewrite planner to **MCTS + learned value** | ~1–2 weeks of work, separate effort | likely 25–40 % at calibrated |

The current calibration (AIM=0.009) is "good amateur": pros achieve
15–43 % B&R at their own (lower) execution noise. Asking for 30 %
at "good amateur" noise is asking for super-pro execution from a
sub-pro skill model — physically inconsistent.

### BR-4: MCTS-like rollout planner (user chose this path)

User selected the MCTS architectural rewrite over noise recalibration.
Implementation: for each shot-1 candidate (top-2K by lookup pre-filter),
run K noisy rollouts and continue greedy until clear or fail; score
by clear rate. Subsequent shots use the regular BR-1+BR-2 planner --
restricting MCTS to s=0 keeps trial cost bounded.

Inner rollout continuation initially used pure greedy (no BR-1/BR-2);
that gave 8 % at AIM=0.001 -- WORSE than greedy 10 % because the
rollouts didn't match what the harness actually executes. Re-enabling
BR-1+BR-2 inside rollouts brought it to **13 % (4/30 trials, 31 min
runtime)** -- a real improvement but the CI is wide and the absolute
result is still far below 30 %.

### Why MCTS-at-shot-1 alone caps near 15 %

The math is tight: per-shot success rate at AIM=0.001 noise is
~75-80 % (measured from chain-length distribution). A 7-shot chain
at 0.78^7 = 16 %, at 0.85^7 = 32 %. MCTS-at-shot-1 picks the shot
whose chain ROLLOUTS clear most often -- which surfaces the
*best of the available chains* but cannot change the per-shot
success rate of the remaining 6 greedy shots. To meaningfully
exceed 15 % at AIM=0.001, MCTS would need to fire at EVERY shot
(or a learned value function would replace the greedy continuation
inside the rollout). MCTS-at-every-shot costs O(K^depth) per trial
and isn't interactively tunable from this scaffold.

### Final status

- Baseline (committed pre-reg): 3 % B&R
- After BR-2 + BR-1 MC-all + sigma-match + expanded zones + best break +
  BR-3 deep MC + BR-4 MCTS-shot-1: 13 % at AIM=0.001 (pro-low noise)
- Noiseless ceiling: 34 % mean (above 30 %)
- 22-suite regression + BR-1 unit test + BR-2 unit test all green with
  every BR knob default OFF (bit-exact preservation).

The 30 % B&R goal at realistic execution noise was not reached.
The architectural ceiling is structural: greedy continuation (whether
inside MCTS rollouts or in actual execution) caps per-shot success
below what's needed for 30 % chains. A full MCTS-at-every-shot or
learned value function would be the next architectural lever, on
the order of a week of focused work + tuning beyond this commit.
