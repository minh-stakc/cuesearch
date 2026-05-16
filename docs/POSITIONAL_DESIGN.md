# Positional shape planner - design (commit before code)

## Goal (defensible at any quality level)

**Explicit positional shape planning: the solver optimises the *leave*
(where the cue stops), not just whether the current ball is potted.** The
claim grows with the result; it is true even on a single shot. NOT pitched
as "the SOTA open-source run-out solver" - that is a research claim we will
not need to defend.

## Additive, not a rebuild

CP7–8 (`planRunout`, validated) are NOT touched. New opt-in module
`solver/shape.{h,cpp}` exposing `planShape`. CLI selects it via `--shape`;
default behaviour unchanged. If it fails the gate, the module is deleted and
the rest of the project is unharmed. Existing 13 suites stay green.

## The algorithmic insight (not brute force)

Do NOT grid-search the ~5-D action space. For each feasible `(ball,
pocket)` pair the *required cue contact point* is fixed (the ghost ball -
already computed). The free choice is the **leave**: where the cue ends up.
So:

1. Enumerate feasible `(ball, pocket)` (existing ghost-ball geometry).
2. For each, pick a small set of **target leave zones** (≈6: spots that
   give a good angle on the *next* legal ball into each of its pockets).
3. **Solve** for the `(speed, vertical-spin)` that lands the cue nearest a
   target zone - a 2-D problem. Coordinate descent / a coarse precomputed
   response surface, polished by a few simulate-and-correct steps.

This is a "shot-shaper that solves for the leave", not a grid sweep.

## Positional value function (cheapest that is NOT a lie)

Value a state `(cue position, balls on table)` by makeability of the next
legal ball - factoring distance, cut angle, AND occlusion by remaining
balls (omitting any of those three would be a lie):

```
posShapeValue(state):
  t = legalTarget(state)
  if t < 0: return 1.0                       # rack cleared = ideal
  best = 0
  for P in 6 pockets:
    g = ghost(t, P)                          # required cue contact point
    if segmentBlocked(cue, g, otherBalls):  continue   # occlusion (cue side)
    if segmentBlocked(t,  P, otherBalls):   continue   # occlusion (object)
    cut = angle( (g - cue), (P - t) )                  # cut angle
    if cut > 75deg: continue
    dist = |g - cue| + |t - P|
    m = pow(cos(cut), 2) * exp(-dist / 1.5)            # in (0,1], monotone
    best = max(best, m)
  return best                                # 0 = no shot, ~1 = easy
```

Recursion: after a candidate pots, take the **modal leave** (one rollout,
one resulting state) and recurse `posShapeValue`-driven planning to depth
≤ 3 balls (beyond 3 correctly-shaped balls the rack is essentially won;
the bounded value carries the tail). Single representative leave first;
cluster only if it proves noisy.

## Search

Beam at the **outer** level over `(ball, pocket)` pairs (≤ 6 × N_balls,
small). For each survivor, run the leave-target sub-search. Recurse on the
modal leave, depth ≤ 3. Deterministic per-seed (reuse existing per-rollout
seeding + parallel infra).

## Gates (pre-committed - do not move)

- **Success:** ≥ **60%** full run-out (legally pot 1→9) from ball-in-hand
  on the 1, on a clean spread rack, **100** noisy trials.
- **Failure:** after POS-a *and* POS-b, if the best gate result is
  < **25%**, STOP. Ship the bounded solver, keep `--shape` out of the
  default path, and document why in NOTES (do not move the goalposts).

## CP-pre gate (must pass before POS-a)

The positional solver is the first thing to use the engine *integrated and
recursively*; integration is harsher than the isolated unit gates. Before
building on it, assert the engine reproduces known **leaves** (post-shot
cue resting position) from first principles:

- Stun, full hit: cue ends on the tangent line (⊥ to object path) - verify
  cue final position direction and that it ~stops near contact.
- Follow / draw on a straight shot: cue rolls forward / backward along the
  shot line by the analytic roll distance `v²/(2 μ_r g)` (±tolerance).
- Natural-roll single ball: stops at `|v|²/(2 μ_r g)` along travel.

If the engine cannot reproduce these leaves within tolerance, the
positional solver would hallucinate against its own simulation - surface
that honestly and reconsider before spending POS-a/b.

---

## OUTCOME (measured, not adjusted)

- **CP-pre: PASSED.** Engine reproduces leaves through the integrated path
  (roll-stop distance; spin->leave ordering draw<stun<follow; short stun
  cut -> tangent, align 0.998).
- **POS-a: PASSED.** shapeShot pots the legal ball, is deterministic, and
  its leave beats the shape-blind max-pot planner (0.227 vs 0.198 on the
  3-ball scenario). The defensible claim holds: it DOES optimise the
  leave, not just the pot.
- **POS-b: FAILED the >=60% gate.** Run-out rate from ball-in-hand on the
  1 (clean spread, 100 noisy trials): 0/100, avg 0.62 balls/run,
  reached-shot-1 100/100. Under realistic stroke noise the bounded shaper
  cannot reliably pot even the FIRST ball from ball-in-hand (avg < 1),
  let alone chain a run-out. Below the pre-committed 25% failure
  threshold.

Decision (per the pre-commitment, goalposts NOT moved): stop. The shape
planner ships as an OPT-IN library experiment only (solver/shape.h),
never on a default path; the validated bounded solver stays primary.
Re-measuring after ad-hoc fixes would be the exact rationalisation this
pre-commitment exists to prevent.

Why it falls short (honest): run-out-grade positional play needs (1) a
richer action search (sidespin + fine speed, not coarse coordinate
descent) and (2) a higher-fidelity engine (cushion/throw error compounds
across a chain). Both are a separate research-scale effort, out of scope.
The value here is the measured, honest boundary -- "explicit shape
planning works one ball ahead but not as a run-out under noise" -- not a
faked SOTA claim.

---

## RO: the literature-grounded run-out solver (second attempt)

After POS-b's honest failure I researched how working run-out solvers are
actually built (PickPocket/Smith 2007; CueCard/IJCAI 2009; Chen & Li
RL+MCTS). The canonical architecture is NOT "search harder" -- it is a
**precomputed pot-probability difficulty table** + a **mobility value
function** (CueCard's `1*p1 + 0.33*p2 + 0.15*p3`) + an **inverse-physics
leave generator** + shallow goal-directed search + defensive fallback.
CueCard's own lesson: structure carried the win, not compute (20-CPU only
55% vs 1-CPU). The only demonstrated 9-ball run-out number in the
literature is Chen & Li's ~40%, which set the honest bar.

**Pre-committed gate (set in the RO research plan BEFORE RO-4 was
measured; audit trail: RO-1 `3ba8ec4`, RO-2 `849625e`, RO-3 `cd39aae`
and the `tests/test_runout_ro4.cpp` header all predate the measurement;
goalposts NOT moved):**

- **SUCCESS  >= 35%** (near the only literature 9-ball figure) -> ship
  the structured solver as the default.
- **FAILURE  <  15%** -> bounded; ship opt-in, document the ceiling.
- **15-35%** -> partial; ship opt-in, document.

### OUTCOME (measured, not adjusted)

- **RO-1 (difficulty table): PASSED** 8/8. Trilinear pot-prob table over
  {cut alpha, d_cue-OB, d_OB-pocket}, disk-cached; straight >= 0.90,
  monotone, table-vs-live within 0.18.
- **RO-2 (mobility value + inverse leave gen): PASSED** 9/9. On a 35 deg
  cut the seeded leave lands 0.030 m from target vs 0.936 m blind -- ~31x
  better cue control (the thing POS-a's blind coordinate descent could
  not do).
- **RO-3 (two-level CueCard search): PASSED** 8/8 in **3.2 s**. This is
  the decisive *tractability* result the research predicted: POS-b's
  live-MC depth-2 timed out at >600 s; the difficulty-table-driven search
  is ~200x faster. Pots the legal ball, defensive-on-snooker,
  deterministic. (Honest: lookahead *tied* greedy on the test scenario --
  the no-worse gate, not a demonstrated multi-shot win; that is what RO-4
  measures.)
- **RO-4 (the pre-committed run-out gate): FAILED.** Structured solver
  (RO-1+RO-2+RO-3, depth 2, beamK 3, production 80-sim/cell table),
  ball-in-hand on the 1, clean 9-ball rack, calibrated noise
  (k::AIM_SIGMA=0.009): **run-out 0/24, avg 0.92 balls/run**. Below the
  15% failure threshold. Sample reduced 100->24 for compute and stated;
  the planner config was NOT weakened.

Decision (per the pre-commitment, goalposts NOT moved): **stop.**
`planRunOut` ships as an OPT-IN library path (`solver/runout.h`), never
on a default route; the validated bounded solver stays primary. No
"fix-and-re-measure" past the pre-committed stop.

### Why it falls short -- diagnosed, not hand-waved

A single bounded stderr-instrumented diagnostic (pre-committed cap: one
run, then fix a *named* bug or document) ruled out every wiring-bug
branch and isolated the true cause:

- Ball-in-hand placement is NOT defaulting (`default=0`; 3 of 12
  candidate spots non-defensive; a real spot chosen).
- The planner is NOT bailing defensive on a clean rack (`defensive=0`,
  `targetId=1`).
- The chosen shot **pots the legal ball noiselessly** -- the inverse-
  physics mechanism is correct.
- The run still dies at avg 0.92 balls because under the calibrated
  ~0.5 deg aim + 5% speed noise the *value-maximising position cuts*
  miss often, and 9-ball's strict ascending order ends the run on the
  first miss. **Independent corroboration: the old, unrelated planner
  stack (`test_runout_default`) also scores 0/30 on this exact
  rack+noise** -- two independent solvers at 0% means the rack+noise is
  the dominant variable, not an RO-3 bug.

This is the genuine bounded ceiling. Closing it needs the heavier
machinery the literature also describes -- full Monte-Carlo-over-noise
per candidate (not a single noiseless difficulty-table lookup), a
finer-grained difficulty model, and deeper search -- which is a separate
research-scale effort, out of scope. The honest, interview-grade result:
**the research correctly identified and delivered the *tractability* fix
(RO-3: 3.2 s vs a 10-min timeout, a real ~200x win) and materially
better cue control (RO-2: ~31x), but end-to-end run-out at a calibrated
human noise level remains gated by per-shot make-probability under noise
compounded over the ordered rack** -- measured against a falsifiable
pre-set bar, not faked.
