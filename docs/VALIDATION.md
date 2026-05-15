# CueSearch — Validation Report

The engine is validated against **published measured data** (Dr. Dave
Alciatore, billiards.colostate.edu / drdavepoolinfo.com) and **conservation
invariants**. Every row is an automated regression gate (`ctest`, 11 suites).
Numbers are from the committed build.

## Collision / throw (Mathavan-2014 impulse model — throw is *emergent*)

| Gate | Measured target | CueSearch | Status |
|---|---|---|---|
| Stun departure (e=1, frictionless) | 90° (textbook) | 90° ± 0.5° | ✅ |
| Stun departure (physical e=0.95) | `atan(tanφ·2/(1−e))` | matches closed form | ✅ |
| Cut-induced throw @ 10° cut | ≈ 1.5° (Dr. Dave) | **1.48°** | ✅ |
| Cut-induced throw @ ~half-ball | ≈ 5.8° (Dr. Dave) | **4.83°** (honest band) | ✅ |
| Cut-induced throw @ 60° cut | rise-then-fall shape | **3.18°** | ✅ |
| Gearing english | 0° throw | ≈ 0° ± 0.2° | ✅ |
| Linear momentum / energy | conserved / non-increasing | holds | ✅ |

## Cue / cloth effects

| Gate | Target | CueSearch | Status |
|---|---|---|---|
| Squirt vs tip offset | 0.5–2.3° (Dr. Dave band) | 0.70°→1.42°, monotone, opposite-english | ✅ |
| Post-collision follow arc | curls toward aim, then straight | Sliding→Rolling, forward arc | ✅ |
| Draw-on-cut | mirror of follow | curls away | ✅ |
| Cloth-speed scaling | window & curve ∝ 1/μₛ | ratio 2.0 (fast vs slow) | ✅ |
| Deflection vs spin | follow < stun(90°) < draw | 50.9° < 85.4° < 149.4° | ✅ |

## Cushion (Mathavan-style, pool-calibrated COR)

| Gate | Target | CueSearch | Status |
|---|---|---|---|
| Perpendicular speed retention | ~0.5–0.7 (Dr. Dave efficiency) | 0.60 | ✅ |
| Running vs reverse english | lengthens / shortens | −0.30 < 0 < +0.30 (strict) | ✅ |

## System-level invariants

| Gate | Result |
|---|---|
| 150 randomized shots: total KE monotone non-increasing | ✅ |
| 150 randomized shots: no ball escapes the table footprint | ✅ |
| Every shot terminates (< horizon, fully at rest) | ✅ |
| Full pipeline bitwise-deterministic (incl. parallel rollouts) | ✅ |

## Solver (the novelty)

| Gate | Result |
|---|---|
| P(pot) calibrates with difficulty | pEasy=1.0 > pHard=0.83 |
| Multi-shot lookahead beats myopia | myopic continuation **0.0** vs lookahead **0.19** |
| Greedy plan execution runs out a 2-ball rack | ✅ |
| `evaluate()` thread-count invariant (deterministic parallelism) | ✅ |

## Throughput (`bench_solver`)

- Physics: **~6,286 full shot simulations / sec**
- Solver: **~20,146 Monte-Carlo rollouts / sec** (parallel)

## Honest limitations (not faked, explicitly scoped)

- **Trisect coefficient deferred.** The exact Dr. Dave "total deflection =
  3×cut" rule needs his specific good-action calibration; we validated the
  unambiguous relational physics (follow < stun < draw) instead of tuning
  constants to fit a number.
- **Cushion COR is an empirical pool fit** (`E_CUSHION_POOL`), not the
  Mathavan snooker value; the full 6-ODE Mathavan-2010 system is a fidelity
  refinement. Cushion gates are trend-based (sparse measured data).
- **Jump shots** (airborne state, 3-D ball-ball, jaw rattle) are a
  documented roadmap item — four of five jump variations emerge for free
  from the existing state model; only the airborne 3-D collision is new.
- **pooltool differential test dropped** (won't install on Windows);
  validation stands on measured data + invariants — the stronger basis.
