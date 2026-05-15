# Positional shape planner — design (commit before code)

## Goal (defensible at any quality level)

**Explicit positional shape planning: the solver optimises the *leave*
(where the cue stops), not just whether the current ball is potted.** The
claim grows with the result; it is true even on a single shot. NOT pitched
as "the SOTA open-source run-out solver" — that is a research claim we will
not need to defend.

## Additive, not a rebuild

CP7–8 (`planRunout`, validated) are NOT touched. New opt-in module
`solver/shape.{h,cpp}` exposing `planShape`. CLI selects it via `--shape`;
default behaviour unchanged. If it fails the gate, the module is deleted and
the rest of the project is unharmed. Existing 13 suites stay green.

## The algorithmic insight (not brute force)

Do NOT grid-search the ~5-D action space. For each feasible `(ball,
pocket)` pair the *required cue contact point* is fixed (the ghost ball —
already computed). The free choice is the **leave**: where the cue ends up.
So:

1. Enumerate feasible `(ball, pocket)` (existing ghost-ball geometry).
2. For each, pick a small set of **target leave zones** (≈6: spots that
   give a good angle on the *next* legal ball into each of its pockets).
3. **Solve** for the `(speed, vertical-spin)` that lands the cue nearest a
   target zone — a 2-D problem. Coordinate descent / a coarse precomputed
   response surface, polished by a few simulate-and-correct steps.

This is a "shot-shaper that solves for the leave", not a grid sweep.

## Positional value function (cheapest that is NOT a lie)

Value a state `(cue position, balls on table)` by makeability of the next
legal ball — factoring distance, cut angle, AND occlusion by remaining
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

## Gates (pre-committed — do not move)

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

- Stun, full hit: cue ends on the tangent line (⊥ to object path) — verify
  cue final position direction and that it ~stops near contact.
- Follow / draw on a straight shot: cue rolls forward / backward along the
  shot line by the analytic roll distance `v²/(2 μ_r g)` (±tolerance).
- Natural-roll single ball: stops at `|v|²/(2 μ_r g)` along travel.

If the engine cannot reproduce these leaves within tolerance, the
positional solver would hallucinate against its own simulation — surface
that honestly and reconsider before spending POS-a/b.
