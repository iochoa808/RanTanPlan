# Factory Robot — Numeric Grounding Analysis

This document details how each action and fluent in the factory-robot domain
exercises a specific feature of the numeric interval bounds system.

## Fluent Bound Analysis

| Fluent | Effects | Freeze result | Bounds at fixpoint |
|--------|---------|---------------|-------------------|
| `energy` | assign to `capacity` (constant), decrease by `work-cost` (constant), decrease by 5 (move), decrease by 20 (turbo-work) | **Upper frozen** at max(capacity) | `[-inf, max(capacity)]` |
| `workload` | increase by 1 (work), increase by 5 (precision-work), increase by 10 (turbo-work), increase by 50/100 (overcharge/overdrive) | **Lower frozen** at 0 | `[0, +inf]` |
| `temperature` | increase by 3 (work), increase by 1 (precision-work), increase by 5 (turbo-work), decrease by `cooling-power` (cool-down) | **Not frozen** — both increased and decreased | `[-inf, +inf]` (directional widening) |
| `production` | assign to 10, 50, 100, 200 (four set-*-power actions) | **Both frozen** at [0, 200] | `[0, 200]` |

## Constant Fluents (Never Modified)

| Fluent | Where used |
|--------|-----------|
| `capacity` | `recharge` effect: `(assign energy capacity)` |
| `work-cost` | `work` precondition and effect, `precision-work` effect |
| `cooling-power` | `cool-down` effect: `(decrease temperature cooling-power)` |
| `max-temp` | `work` and `precision-work` preconditions: `(<= temperature max-temp)` |
| `efficiency` | `precision-work` precondition: `(>= energy (* workload efficiency))` |

## Action-by-Action Grounding Feature Map

### `recharge` — Ceiling freezing (Case 1)
- Effect: `(assign (energy ?r) (capacity ?r))`
- `capacity` is constant → assign to constant expression
- This is what makes `energy` upper-freezable: the assign introduces a bounded ceiling
- Without freezing: energy still gets upper-frozen by directional widening (decreases dominate)
- With freezing: upper bound is precisely `max(capacity across robots)` regardless of threshold
- Precondition: `(has-charger ?s)` — restricts recharging to charging stations, creating heuristic plateaus (robot must move to charger before recharging, but the move doesn't directly improve any goal h^add estimate)

### `work` — Floor freezing + constant reference + calibration (Cases 2, 7)
- Effect: `(increase (workload ?r) 1)` — workload only increases → lower frozen at 0
- Effect: `(decrease (energy ?r) (work-cost ?r))` — constant fluent in effect expression
- Precondition: `(>= (energy ?r) (work-cost ?r))` — arithmetic comparison with constant
- Precondition: `(<= (temperature ?r) (max-temp ?r))` — prunable when temp exceeds max
- Precondition: `(calibrated ?r)` — requires calibration (cleared by turbo-work), forcing detour to calibration station after turbo-work

### `cool-down` — Directional widening (Case 4)
- Effect: `(decrease (temperature ?r) (cooling-power ?s))` — pushes temperature down
- Combined with `work`/`precision-work`/`turbo-work` which push temperature up
- Both directions → neither side frozen → directional widening applies
- Result: temperature widens to `[-inf, +inf]`

### `set-low-power`, `set-medium-power`, `set-high-power`, `set-turbo` — Multi-assign freezing (Case 3)
- Effects: assign production to 10, 50, 100, 200 respectively
- All four are assigns to constant literal values
- **Without freezing**: upper expansion count reaches 4 (from 0→10→50→100→200), exceeds threshold 3, production widens to `[0, +inf]`
- **With freezing**: all assigns are to constants → both sides frozen at `[0, max(0, 10, 50, 100, 200)]` = `[0, 200]`
- This is the key case that demonstrates freezing's advantage over pure directional widening

### `overdrive` — Pruning via frozen multi-assign (Case 6)
- Precondition: `(>= (production ?r) 250)`
- Production frozen at `[0, 200]` → `200 < 250` → **PRUNED**
- **Only pruned with freezing!** Without freezing, production = `[0, +inf]`, 250 is satisfiable → not pruned
- Each robot generates one ground instance → 2 pruned per typical problem (with 2 robots)

### `overcharge` — Pruning via frozen ceiling (Case 5)
- Precondition: `(>= (energy ?r) 200)`
- Energy upper frozen at `max(capacity)` (typically 120) → `120 < 200` → **PRUNED**
- Pruned by basic numeric grounding (not specific to freezing), since directional widening also keeps the upper bound finite for energy

### `precision-work` — Interval arithmetic + calibration (Case 8)
- Precondition: `(>= (energy ?r) (* (workload ?r) (efficiency ?r)))`
- LHS: `energy = [-inf, max(capacity)]`
- RHS: `workload = [0, +inf]` × `efficiency = [min_eff, max_eff]` = `[0, +inf]`
- Satisfiable check: `energy.upper >= product.lower` → `max(capacity) >= 0` → **satisfiable → NOT pruned** (correct)
- This exercises the interval multiplication logic in `evaluate_interval()`
- Precondition: `(calibrated ?r)` — same calibration requirement as `work`

### `move` — Boolean + numeric precondition + mutual exclusion
- Precondition: `(at ?r ?s1) AND (connected ?s1 ?s2) AND (free ?s2) AND (>= (energy ?r) 5)`
- Boolean part handled by `FactIndex`, numeric part by `NumericBoundsIndex`
- Energy lower bound is `-inf` and upper is `max(capacity)`, so `energy >= 5` is satisfiable
- `(free ?s2)` enforces mutual exclusion: only one robot per station. h^add misses the contention — under delete-relaxation, `free` is never consumed, so all robots appear to move freely. In reality, robots must coordinate to avoid blocking each other.
- Effect: `(free ?s1)` / `(not (free ?s2))` — vacating frees old station, occupying locks new one

### `turbo-work` — Correctly NOT pruned + decalibration
- Precondition: `(>= (production ?r) 150)`
- Production frozen at `[0, 200]` → overlap with `[150, +inf)` exists → **satisfiable → NOT pruned**
- Verifies that freezing doesn't over-prune: the turbo mode (200) satisfies the threshold (150)
- Effect: `(not (calibrated ?r))` — decalibrates robot, forcing a detour to calibration station before regular/precision work can resume. h^add underestimates this cost because the calibrate action looks cheap in isolation (ignoring the required move to a calibrator station).

### `calibrate` — Recalibration after turbo-work
- Precondition: `(has-calibrator ?s) AND (not (calibrated ?r))`
- Effect: `(calibrated ?r)`
- Only available at stations with calibration equipment (cooling station)
- Creates forced detours that are invisible to h^add: after turbo-work, the robot must move to the cooling station, calibrate, then move back to resume work. Each intermediate step looks like regression to the heuristic.

## h^add / GBFS Anti-Patterns

This domain is designed to create scenarios where h^add + GBFS struggles:

### Station-restricted recharging (Change 3)
`recharge` requires `(has-charger ?s)`, only available at the charging station. When a robot runs out of energy at an assembly station, h^add sees the remaining cost as roughly `h(move) + h(recharge) + h(move-back) + h(work)`, but each intermediate step doesn't decrease the workload goal's h^add. GBFS sees a plateau of length 2-3 before any progress.

### Mutual exclusion (Change 4)
`move` requires `(free ?s2)`. Under delete-relaxation, `free` is never deleted (since deletes are ignored), so h^add thinks every station is always free. In reality, with N robots and N+1 stations, robots must coordinate — sometimes one robot has to move out of the way for another. h^add completely misses this contention, leading to plans that are infeasible in practice. GBFS commits to moving one robot, then discovers the target is occupied and must backtrack.

### Required calibration (Change 5)
`turbo-work` deletes `(calibrated ?r)`, and `work`/`precision-work` require it. The calibrate action is only at the cooling station. After turbo-work, the robot must: move to cooling → calibrate → move back. This is a 3-step detour where every step looks like regression (no workload gain). h^add at the pre-turbo state sees turbo-work as yielding +10 workload cheaply, but doesn't account for the mandatory calibration detour afterward. GBFS greedily chains turbo-works, then finds regular work blocked.

### Combined effect
With all three changes active, the plan structure requires cyclic station visits: assembly (work) → charging (recharge) → cooling (cool + calibrate) → assembly (work). Each transition is a "non-productive" move that h^add undervalues. For multi-robot instances, the mutual exclusion forces serialization of these cycles, further widening the gap between h^add's estimate and actual plan cost.

## Numeric Goal Checking (Case 9)

Goals like `(>= (workload r1) 12)` and `(<= (temperature r1) 3)` are checked against
the numeric bounds at fixpoint:
- `workload = [0, +inf]` → `upper >= 12` → satisfiable
- `temperature = [-inf, +inf]` → always satisfiable

## Scaling Behavior

| Robots (R) | Stations (S) | Ground actions | Pruned | Notes |
|------------|-------------|---------------|--------|-------|
| 2 | 3 | ~60 | 4 | 2 overdrive + 2 overcharge |
| 4 | 5 | ~250 | 8 | 4 overdrive + 4 overcharge |
| R | S | O(R×S² + R) | 2R | Pruned count scales linearly with robots |

Constraint: `R <= S` (mutual exclusion requires at least one free station for movement).
