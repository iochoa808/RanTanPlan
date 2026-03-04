# Reachability-Based Grounding in RantanPlan

The goal of grounding in automated planning is to take "lifted" action schemas that have parameters (e.g., `drive(?vehicle, ?from, ?to)`) and generate all the concrete, specific instances of these actions (e.g., `drive(truck1, cityA, cityB)`) that could *possibly* be executed while trying to solve the problem.

Replacing parameters with every combination of objects creates an astronomically huge number of actions. To avoid this, RantanPlan uses a **"reachability-based" delete-relaxation fixpoint algorithm**, which only grounds actions that could hypothetically be reached from the initial state.

Here is a step-by-step explanation of how the grounder executes this, interleaved with a concrete scenario (a simple truck-driving problem) highlighting how explicit facts and **numeric fluents** (like fuel, distance, capacity) are handled.

---

## Part 1: Boolean-Only Grounding (Default)

### The Scenario: A Truck Delivery
Imagine we have a lifted action schema:
`drive(?vehicle, ?from, ?to)`
* **Preconditions**: `at(?vehicle, ?from)`, `connected(?from, ?to)`, `not (broken(?vehicle))`, and `fuel(?vehicle) >= 10`.
* **Effects**: Add `at(?vehicle, ?to)`, Delete `at(?vehicle, ?from)`, Decrease `fuel(?vehicle)` by `10`.

### Step 1: The Initial State and Closed-World Assumption
Before the loop starts, the system looks at the initial state to build a database of known facts (the **`FactIndex`**).

* **Example Action taken**: Suppose the initial state says `at(truck1, cityA) = true`, `connected(cityA, cityB) = true`, and `fuel(truck1) = 50`.
* **The Boolean Rule**: Using the Closed-World assumption, it saves the explicit `true` facts: `at(truck1, cityA)` and `connected(cityA, cityB)`. Anything unmentioned (like `at(truck1, cityB)`) is assumed to be `false`.
* **The Numeric Rule**: **Numeric assignments are completely ignored.** The grounder skips `fuel(truck1) = 50`. Why? Because this phase is purely a logical/structural reachability check. Tracking exact numeric bounds is pushed off to a separate, later step (the Numeric Relaxed Planning Graph, or NRPG).

### Step 2: Extracting Preconditions
The grounder enters its main loop. It inspects our `drive(?vehicle, ?from, ?to)` schema to see what conditions are required to execute it.

* **Example Action taken**: It pulls out the positive boolean atoms: `at(?vehicle, ?from)` and `connected(?from, ?to)`.
* **The Negative Rule**: It ignores `not (broken(?vehicle))` because of *delete-relaxation*. In a relaxed world, we assume negative requirements are always satisfiable because we pretend bad things never happen.
* **The Numeric Rule**: It completely ignores the numeric precondition `fuel(?vehicle) >= 10`! Again, boolean reachability assumes that any numeric requirement can *theoretically* be met, so as not to accidentally discard an action that might actually be reachable.

### Step 3: Finding Bindings (The "Database Join")
Now, the **`BindingMatcher`** tries to find the actual objects that match these extracted requirements by treating the `FactIndex` like SQL database tables.

* **Example Action taken**: It looks at the `at` table and sees `at(truck1, cityA)`. So it binds `?vehicle = truck1` and `?from = cityA`.
* It then looks at the `connected` table for `connected(cityA, ?to)`. It finds `connected(cityA, cityB)`, so it binds `?to = cityB`.
* If a parameter wasn't tied to a precondition (say there was a `?driver` parameter but no precondition restricted it), it just guesses every single person in the problem that fits the `driver` type.

### Step 4: Instantiating and Deduplicating Actions
The combination of objects is grouped into a `PartialBinding`. The system checks if it has processed this exact action before.

* **Example Action taken**: The system creates a fingerprint for `drive(truck1, cityA, cityB)`. Since it is the first time seeing this combination, it instantiates the actual ground action and adds it to the master list of possible actions.

### Step 5: Collecting "Add Effects" (Delete-Relaxation)
As soon as `drive(truck1, cityA, cityB)` is instantiated, the system checks what effects that action causes to see what *new* facts become possible.

* **Example Action taken**: The effects are: Add `at(truck1, cityB)`, Delete `at(truck1, cityA)`, Decrease `fuel(truck1)` by `10`.
* **The Delete-Relaxation Rule**: The "Delete `at(truck1, cityA)`" is ignored. In our relaxed analysis world, objects can be in multiple places at once; we only care about *accumulating* truths.
* **The Numeric Rule**: "Decrease `fuel(truck1)` by `10`" is completely bypassed. Numeric effects (increase, decrease, assign numbers) cannot produce boolean `true` facts, so they do nothing to expand our `FactIndex`.
* **The Result**: Only "Add `at(truck1, cityB)`" matters. It is successfully registered as a newly accessible fact and inserted into the `FactIndex`.

### Step 6: The Fixpoint Loop
Because Step 5 added a *new* fact (`at(truck1, cityB)`) to the `FactIndex`, the system knows it needs to run the entire loop again.

* **Example Action taken**: On Iteration 2, when checking preconditions, the `BindingMatcher` now sees `at(truck1, cityB)`.
* Because of this, if `connected(cityB, cityC)` is in the `FactIndex`, it can now successfully bind and instantiate the action `drive(truck1, cityB, cityC)`.
* This action adds `at(truck1, cityC)`. It triggers Iteration 3.
* The loop continues expanding the frontier until an entire loop finishes without discovering a single new fact. This is called the **Fixpoint**.

### Step 7: Goal Reachability Check
At the end of an iteration, the grounder checks if the user's Goals are met by the current `FactIndex`.

* **Example Action taken**: Suppose the goal is `at(truck1, cityC) AND fuel(truck1) >= 5`.
* **The Numeric Rule**: It only checks if the boolean `at(truck1, cityC)` has been reached. It skips checking `fuel(truck1) >= 5` because boolean reachability doesn't know what the numbers are.
* If `at(truck1, cityC)` never enters the `FactIndex` by the time we hit the fixpoint, the grounder returns `proven_unsolvable = true` and perfectly aborts the program early! If the truck can't get to City C even when it has infinite fuel and can be in multiple places at once, it definitely can't do it in the real constraints.

---

## Part 2: Numeric Interval Grounding (`--numeric-grounding`)

The boolean reachability grounder generates **over-approximations**. Because it pretends numbers don't exist, it instantiates actions that are logically "reachable" but numerically impossible.

By tracking **numeric bounds** (intervals like `[min, max]`) using Interval Arithmetic alongside boolean facts, the grounder can actively prune dead-end actions before they ever reach the solver. This is enabled with the `--numeric-grounding` flag.

### Architecture Overview

The numeric grounding system adds three components to the boolean grounder:

1. **`NumericBoundsIndex`** — Parallel to `FactIndex`, maps each ground numeric fluent to a closed interval `[lower, upper]`. Supports convex union updates and directional widening.
2. **`Interval`** — Lightweight `[lower, upper]` struct with full interval arithmetic (`+`, `-`, `*`, `/`, `convex_union`, `overlaps`).
3. **`IntervalEvaluator`** — Two functions:
   * `evaluate_interval(ExprID)` — Recursively evaluates a numeric expression tree to an `Interval`.
   * `numeric_precondition_satisfiable(ExprID)` — Walks a precondition tree, evaluating numeric comparisons against current bounds. Returns `false` only if provably unsatisfiable.

### The Scenario: A Truck with Limited Fuel
Let's upgrade our truck scenario.
* **Initial State**: `at(truck1, cityA) = true`, `connected(cityA, cityB) = true`, `has_gas_station(cityA) = true`, and crucially, **`fuel(truck1) = 5`**. (The truck starts almost empty).
* **Action 1**: `drive(?vehicle, ?from, ?to)`
  * *Preconds*: `at(?vehicle, ?from)`, `connected(?from, ?to)`, `fuel(?vehicle) >= 10`
  * *Effects*: Add `at(?vehicle, ?to)`, Decrease `fuel(?vehicle)` by `10`.
* **Action 2**: `refuel(?vehicle, ?loc)`
  * *Preconds*: `at(?vehicle, ?loc)`, `has_gas_station(?loc)`
  * *Effects*: Assign `fuel(?vehicle) = 50`.

Here is how the Interval-Arithmetic enhanced grounder processes this.

### Step 1: Initializing the Numeric FactIndex
Instead of throwing away the initial numbers, the grounder establishes starting bounds for every numeric fluent.
* **The Boolean Rule**: `at(truck1, cityA)` and `connected(cityA, cityB)` are true.
* **The Interval Rule**: It creates a tracker for fuel: `fuel(truck1) = [5, 5]`. The minimum and maximum hypothetically reachable fuel is currently `5`.

### Step 2: Evaluating Numeric Preconditions (The Pruning)
In Iteration 1, the grounder tries to bind the `drive` action exactly like before. The boolean requirements match.
* **The old grounder** would say: "Great! I'm instantiating `drive(truck1, cityA, cityB)`!"
* **The Interval grounder** pauses and checks the numeric precondition: `fuel(truck1) >= 10`.
* It looks at the tracker: `max(fuel(truck1))` is `5`. Since `5 >= 10` is mathematically impossible given our current upper bound, **the action is temporarily blocked**.
* Crucially, the action is **not** added to the `seen_bindings` set. This means it will be retried in later iterations if bounds widen.

### Step 3: Resolving via Numeric Effects
`has_gas_station(cityA) = true` is in the initial state, so:
* In Iteration 1, the grounder successfully instantiates `refuel(truck1, cityA)` because its boolean preconditions are met.
* **The Interval Effect Rule**: The grounder evaluates the effects of `refuel`. The effect is `Assign fuel = 50`.
* Instead of strictly overwriting the value as it would in normal execution, the grounder updates its interval tracker by taking the **convex union** of the old bounds and the newly assigned value. `fuel(truck1)` goes from `[5, 5]` to `[min(5, 50), max(5, 50)]` which is `[5, 50]`. (This is standard ARPG continuous relaxation: variables remember all reachable values).

### Step 4: The Inner Numeric Fixpoint
After processing all new actions in an iteration, the grounder runs an **inner fixpoint loop** that re-applies all accepted actions' numeric effects until bounds stabilize. This is necessary because numeric effects are **not idempotent**: e.g., `increment(x)` adds `rate(x)` to `value(x)`, and if `rate(x)` widened since the action was first processed, `value(x)` must widen too.

### Step 5: Unlocking Actions in Later Iterations
Because bounds changed (either from new actions' effects or from the inner fixpoint), the grounder triggers a new iteration and retries previously-pruned bindings.
* It looks at `drive(truck1, cityA, cityB)` again.
* It checks the numeric precondition: `fuel(truck1) >= 10`.
* It looks at the tracker: `max(fuel(truck1))` is now `50`. Since `50 >= 10` is theoretically possible within the interval `[5, 50]`, the action is **finally unlocked and instantiated**.
* The action is now added to `seen_bindings` and its entry is removed from the `numeric_pruned_bindings` tracker (it is no longer permanently pruned).

### Step 6: Goal Reachability with Numeric Bounds
The numeric grounder also checks numeric comparisons in goals. If the goal includes `fuel(truck1) >= 5` and the tracker shows `fuel(truck1) = [3, 3]`, the grounder can prove the goal is unreachable and declare the problem unsolvable.

### Step 7: Permanent vs Temporary Pruning
At the fixpoint, some actions may still be in the `numeric_pruned_bindings` map — these were pruned in every iteration and never unlocked. These are **permanently pruned** and are reported in the log output. Actions that were temporarily pruned but later accepted are not counted.

---

## Part 3: Directional Widening

### The Termination Problem
Numeric effects can create self-referential feedback loops. Consider `(decrease (fuel ?v) 10)`:

1. `fuel = [5, 50]`. Apply decrease: `[5, 50] - [10, 10] = [-5, 40]`. Union: `[-5, 50]`. Changed.
2. Re-apply: `[-5, 50] - [10, 10] = [-15, 40]`. Union: `[-15, 50]`. Changed.
3. Re-apply: `[-15, 50] - [10, 10] = [-25, 40]`. Union: `[-25, 50]`. Changed.
4. This will never converge — the lower bound keeps decreasing forever.

To guarantee termination, the grounder applies **widening**: after a fluent's bounds have expanded a configurable number of times (default: 3), snap the moving side to infinity.

### Naive (Symmetric) Widening
The simplest approach widens both sides to `[-inf, +inf]` after 3 total expansions. This is safe but destroys all pruning information. A fluent at `[-inf, +inf]` makes every numeric precondition trivially satisfiable.

### Directional Widening (What We Actually Do)
Instead of a single expansion counter, we track **per-side counters**: `lower_expansion_count` and `upper_expansion_count`. Each counter only increments when its respective side actually moves:

| Step | Interval   | Lower count | Upper count | What moved |
|------|-----------|-------------|-------------|------------|
| Init | `[5, 5]`   | 0           | 0           | —          |
| Refuel assign | `[5, 50]` | 0 | 1 | Upper grew |
| Decrease | `[-5, 50]` | 1 | 0 | Lower shrank |
| Re-decrease | `[-15, 50]` | 2 | 0 | Lower shrank |
| Re-decrease | `[-25, 50]` | 3 | 0 | Lower hits threshold |
| **Widen** | **`[-inf, 50]`** | — | — | Only lower side widened! |

The upper bound stays at `50` because it only moved once (the refuel assign). The lower bound hits the threshold and gets widened to `-inf`.

**Result**: `fuel(truck1) = [-inf, 50]`. The upper bound `50` is preserved — it reflects the maximum fuel achievable through refueling.

### Why This Matters
With symmetric widening, `fuel = [-inf, +inf]` and the precondition `fuel >= 10` evaluates as: `upper(+inf) >= 10` — always satisfiable. No pruning possible.

With directional widening, `fuel = [-inf, 50]` and the precondition `fuel >= 10` evaluates as: `upper(50) >= 10` — still satisfiable. But consider an aircraft with `capacity = 3000` and a fast-flight precondition requiring `fuel >= 5000`. The interval `[-inf, 3000]` has `upper(3000) < 5000`, so **the action is permanently pruned**. This is exactly what happens in the zenotravel domain.

---

## Part 4: When Does Numeric Pruning Help?

The key question: under what conditions does interval-based pruning actually remove actions that the boolean grounder cannot?

### Case 1: Constant-Only Preconditions (Both Sides Constant)
**Maximum pruning power.** When a precondition compares two fluents that are never modified by any effect, their bounds remain at their initial point values forever.

**Example** — Transport domain with weight limits:
```
(:action load
  :precondition (<= (weight ?cargo) (capacity ?vehicle))
  :effect (loaded ?cargo ?vehicle))
```
With `weight(heavy) = 15` and `capacity(small-truck) = 5`, neither fluent is ever modified. The intervals are `[15, 15]` and `[5, 5]`. The precondition `15 <= 5` is provably false. The action `load(heavy, small-truck)` is **permanently pruned**.

**Other examples**: minimum crew requirements (`crew-size >= min-crew`), tool compatibility checks (`tool-strength >= material-hardness`), vehicle speed limits (`max-speed >= required-speed`).

### Case 2: Assign-to-Constant Effects (One Side Stabilizes)
When a fluent is only modified by `assign` to a constant value (not self-referential like `assign x = x + 1`), its bounds converge quickly without hitting the widening threshold.

**Example** — Workshop domain with certification levels:
```
(:action certify
  :effect (assign (certification ?worker) 5))

(:action do-task
  :precondition (>= (certification ?worker) (required_level ?task)))
```
After the `certify` action, `certification` bounds become `[0, 5]` (union of initial `[0, 0]` and assigned `[5, 5]`). Re-applying the effect yields the same `[5, 5]`, so no further expansion — the interval stabilizes at `[0, 5]`.

If `required_level(expert-job) = 15`, then `upper(5) < 15` and `do-task(*, expert-job, *)` is **permanently pruned** for all workers.

### Case 3: Monotonic Fluents with a Ceiling (Directional Widening)
When a fluent has both increase/decrease effects (which cause widening) AND an assign-to-constant effect (which caps one side), directional widening preserves the cap.

**Example** — Zenotravel with aircraft fuel:
```
(:action fly-fast
  :precondition (>= (fuel ?a) (* (distance ?c1 ?c2) (fast-burn ?a)))
  :effect (decrease (fuel ?a) (* (distance ?c1 ?c2) (fast-burn ?a))))

(:action refuel
  :precondition (> (capacity ?a) (fuel ?a))
  :effect (assign (fuel ?a) (capacity ?a)))
```
The `decrease` effect pushes the lower bound down repeatedly (widened to `-inf`). The `assign` to `capacity` sets a ceiling. With directional widening, `fuel(?a)` converges to `[-inf, capacity(?a)]`.

If a small aircraft has `capacity = 3000` but `distance * fast-burn = 5000` for some city pair, then `upper(3000) < 5000` and `fly-fast(small-plane, cityX, cityY)` is **permanently pruned**. The plane simply cannot carry enough fuel for fast flights on long routes, regardless of how many times it refuels.

### Case 4: Resource Budgets with Fixed Income
When a resource has a fixed production rate (constant increment) but the precondition requires more than the maximum achievable value.

**Example** — Energy domain with solar panels:
```
(:action generate
  :effect (increase (energy ?base) (panel-output ?base)))  ;; constant

(:action fire-laser
  :precondition (>= (energy ?base) 1000))
```
`energy` gets increased by `panel-output` which is constant. The increase effect creates a self-referential loop: `[0, 0] + [50, 50] = [50, 50]`, union → `[0, 50]`, then `[0, 50] + [50, 50] = [50, 100]`, union → `[0, 100]`, etc. The upper bound keeps growing, eventually widening to `+inf`. So `fire-laser` is NOT pruned — which is correct, because in theory energy can accumulate.

However, if there's also a `(decrease (energy ?base) (drain-rate ?base))` effect with `drain-rate = 60` and `panel-output = 50`, the net effect is ambiguous under interval arithmetic — both sides widen to infinity. This is a limitation: interval arithmetic cannot reason about net rates.

### Case 5: Numeric Goals
The numeric grounder also checks numeric comparisons in goals. If the goal is `total-cost <= 100` and the cost can only increase (lower bound grows), then once `lower(total-cost) > 100`, the goal is provably unreachable and the problem is declared unsolvable early.

### Summary: When Pruning Works Best

| Pattern | Pruning power | Why |
|---------|--------------|-----|
| Both sides constant | Maximum | Intervals are exact points, never widen |
| Assign to constant (no self-ref) | Strong | Bounds converge in 1-2 steps |
| Decrease + assign ceiling | Good (with directional widening) | Upper bound preserved |
| Increase + assign floor | Good (with directional widening) | Lower bound preserved |
| Self-referential increase/decrease only | None | Both sides widen to infinity |
| Multiplication/division effects | Weak | Intervals grow exponentially fast |

---

## Part 5: Challenges and Limitations

### The Assignment Trap (Interval Weakening)
Because of the monotonic (delete-relaxed) nature of reachability generation, variables can never "forget" past states. This makes *assignments* that reference the variable itself actively detrimental to pruning.
* If a problem has `(assign x (+ x 1))`, the interval grows unboundedly: `[0, 0]` → `[0, 1]` → `[0, 2]` → ... → widened to `[0, +inf]`.
* This is equivalent to `(increase x 1)` — the syntactic form doesn't matter; the self-referential semantics cause the same expansion.
* Only `(assign x 50)` — assignment to a value that doesn't reference `x` — avoids this trap.

### The Dependency Problem (Over-Approximation Blowout)
When variables appear multiple times in a non-linear equation, standard interval arithmetic evaluates each instance independently, losing the mathematical correlation between them.
* **Example:** `x * x <= 10`, and our interval for `x` is `[-5, 5]`.
* Standard interval arithmetic calculates: `[-5, 5] * [-5, 5]`.
* The result is the interval `[-25, 25]`. It concludes "Yes! `-25` is less than `10`, so this precondition *might* be reachable! Unblock the action!"
* Mathematical reality: any real number squared is positive. The true range of x^2 for x in [-5, 5] is `[0, 25]`. The interval calculation produced a massive over-approximation, making the pruning far less effective.

### Division by Zero Traps
If an interval happens to straddle zero (e.g., `speed = distance / time` where `time = [-2, 5]`), interval division operations result in infinites (`[-inf, inf]`). This essentially breaks the bounds tracker for that variable.

### Conditional Effects
Under delete-relaxation, conditional effects must be processed with the assumption that the condition **can** hold (over-approximation). Skipping conditional effects would under-approximate the reachable interval — e.g., missing a fuel decrease would keep the lower bound too high, causing unsound pruning of refuel actions that check `capacity > fuel`. The grounder processes all conditional effects regardless of whether their conditions are currently satisfiable.

### Graceful Degradation
Because of these issues, interval arithmetic is a lightweight continuous relaxation — it sacrifices precision for speed. When faced with complex non-linear math, the intervals simply widen so much that the grounder gracefully degrades back into behaving exactly like the purely boolean grounder. No action that should be reachable is ever incorrectly pruned (soundness is guaranteed by over-approximation).

---

## Part 6: Ceiling/Floor Freezing

Directional widening (Part 3) already preserves finite bounds on the side that isn't moving. But it still relies on an arbitrary **threshold** (default: 3 expansions). This creates a tension:

* **Low threshold** (e.g., 3): Fast convergence — the inner fixpoint loop runs only a few iterations for self-referential effects. But a fluent with 4+ distinct constant-assign values will have its bound widened to infinity prematurely.
* **High threshold** (e.g., 1000): Preserves precise bounds for multi-assign fluents. But the inner fixpoint loop runs ~1000 iterations for every self-referential effect (e.g., `decrease fuel 10`), making grounding dramatically slower for no pruning benefit — `[-inf, 50]` and `[-9950, 50]` give identical pruning results.

**Ceiling/Floor Freezing** eliminates this tension entirely. The idea: before the fixpoint loop, statically analyze all effects to determine, for each numeric fluent, whether a finite ceiling (upper freeze) or floor (lower freeze) exists. Frozen bounds are never widened, regardless of the expansion count. Self-referential effects still use the threshold for their side. This gives the precision of an infinite threshold for constant-assigns with the speed of threshold-3 for everything else.

### How It Works

**Step 1: Identify constant fluents.** A numeric fluent is *constant* if no action has an effect (assign, increase, or decrease) that modifies it. Examples: `weight(?cargo)`, `capacity(?vehicle)`, `distance(?c1, ?c2)`, `fast-burn(?aircraft)`.

**Step 2: For each non-constant fluent, classify its effects.** For every effect that targets fluent F:

* **Assign to constant expression**: The value expression references only constant fluents (never modified). Example: `(assign (fuel ?a) (capacity ?a))` where `capacity` is constant. The evaluated interval is a finite, fixed range. This contributes a **ceiling candidate** (the upper bound of the evaluated interval) and a **floor candidate** (the lower bound).

* **Increase with known-positive delta**: `(increase F delta)` where `delta` evaluates to an interval with `lower > 0` (using only constant fluents). This can only push F upward — it cannot lower the lower bound. The upper bound has no ceiling from this effect (it grows unboundedly).

* **Increase with known-negative delta**: `(increase F delta)` where `delta` evaluates to an interval with `upper < 0`. This is effectively a decrease — it can only push F downward. The lower bound has no floor from this effect, but the upper bound is not raised.

* **Decrease with known-positive delta**: `(decrease F delta)` where `delta` evaluates to `lower > 0`. Can only push F downward. No floor, but no ceiling impact.

* **Decrease with known-negative delta**: `(decrease F delta)` where `delta` evaluates to `upper < 0`. Effectively an increase. Can only push F upward.

* **Unknown sign or self-referential delta**: The delta expression references a non-constant fluent, or the evaluated interval straddles zero. Cannot determine direction — no freezing possible for this effect.

**Step 3: Determine frozen bounds.**

* **Freeze upper bound** if: every effect on F either (a) assigns to a constant expression, or (b) is an increase/decrease that provably cannot raise F (i.e., known-positive decrease or known-negative increase). The frozen upper bound is `max(initial_value, max of all assign-ceiling candidates)`.

* **Freeze lower bound** if: every effect on F either (a) assigns to a constant expression, or (b) is an increase/decrease that provably cannot lower F (i.e., known-positive increase or known-negative decrease). The frozen lower bound is `min(initial_value, min of all assign-floor candidates)`.

* If any effect has unknown sign or is self-referential in a direction that could push a bound, that side is **not frozen** and falls back to directional widening with threshold.

### The Sign Problem

The classification above requires knowing the sign of each delta expression. This is straightforward when the delta is a literal constant (e.g., `10`) or a product of constant fluents (e.g., `distance * slow-burn`). But it becomes ambiguous when:

* **The delta references a non-constant fluent**: `(increase (energy ?b) (production-rate ?b))` — if `production-rate` is modified by some other action, its sign is unknown.
* **The delta interval straddles zero**: `(decrease (position ?r) (velocity ?r))` where `velocity` can be negative (moving backward). The decrease could push position up or down depending on the sign of velocity.
* **The delta involves subtraction**: `(increase (balance ?a) (- (income ?a) (expenses ?a)))` — even if both are constants, the result could be positive or negative depending on the specific object.

In all ambiguous cases, the safe choice is: **do not freeze that side**. The bound falls back to directional widening with threshold, which is still sound.

### Walkthrough: Zenotravel Fuel

Analyzing `fuel(?aircraft)`:

1. **Effects on fuel**:
   * `(assign (fuel ?a) (capacity ?a))` — `capacity` is constant → assign to constant expression. Ceiling candidate = `capacity(?a)`.
   * `(decrease (fuel ?a) (* (distance ?c1 ?c2) (slow-burn ?a)))` — `distance` and `slow-burn` are both constant. Evaluate delta: `[dist_min * burn_min, dist_max * burn_max]`. All values are positive (distances and burn rates are positive). So this is a **known-positive decrease** — pushes fuel downward only.
   * `(decrease (fuel ?a) (* (distance ?c1 ?c2) (fast-burn ?a)))` — same analysis, known-positive decrease.

2. **Upper bound freezing**: Every effect either assigns to a constant ceiling (`capacity`) or decreases (cannot raise fuel). Freeze upper at `max(initial_fuel, capacity)` = `capacity(?a)`.

3. **Lower bound**: Decreases push it down indefinitely → not frozen (falls back to directional widening → `-inf`).

4. **Result**: `fuel(?a) = [-inf, capacity(?a)]` — identical to what directional widening already gives, but now guaranteed correct regardless of threshold. Even if fuel had 100 different assign-to-constant effects, the upper bound would stay at the max of all those constants instead of widening to `+inf`.

### Walkthrough: Multi-Assign Edge Case

Consider a domain where a robot's `mode` fluent gets assigned to different constant values by different actions:

```
(:action set-low-power   :effect (assign (power-level ?r) 10))
(:action set-medium-power :effect (assign (power-level ?r) 50))
(:action set-high-power   :effect (assign (power-level ?r) 100))
(:action set-turbo        :effect (assign (power-level ?r) 200))
```

With directional widening (threshold 3): upper expansion count hits 4 (from 0→10→50→100→200), exceeds threshold, and widens to `+inf`. A precondition like `power-level >= 150` would not be pruned, even though only `set-turbo` can satisfy it.

With ceiling freezing: all effects are assigns to constants. Ceiling = `max(0, 10, 50, 100, 200)` = `200`. The upper bound is frozen at `200`. The precondition `power-level >= 150` correctly evaluates as satisfiable (`200 >= 150`). But `power-level >= 300` would be pruned (`200 < 300`).

### Summary: Freezing vs Directional Widening

| Scenario | Directional Widening (threshold 3) | Ceiling/Floor Freezing |
|----------|-------------------------------------|----------------------|
| 1 constant-assign + decreases | `[-inf, ceiling]` | `[-inf, ceiling]` (same) |
| 2 constant-assigns + decreases | `[-inf, max-ceiling]` | `[-inf, max-ceiling]` (same) |
| 4+ constant-assigns | Upper widens to `+inf` | `[-inf, max-ceiling]` (better) |
| Self-referential increase only | Upper widens to `+inf` (fast, 3 iters) | Upper widens to `+inf` (fast, 3 iters) |
| Mixed unknown-sign effects | Falls back to widening | Falls back to widening (same) |
| Inner fixpoint speed | ~3 iterations per fluent | ~3 iterations per fluent (same) |

Freezing is strictly more precise than directional widening, at the cost of a one-time pre-analysis pass over all effects. The two techniques compose: freezing handles the constant-assign case precisely, and directional widening handles everything else as a fallback.

---

## Part 7: Implementation Details

### Key Design Decisions

1. **Enabled by default, disable with `--no-numeric-grounding`.** All numeric logic can be disabled. When disabled, behavior is identical to the boolean-only grounder — zero overhead.

2. **Pruned actions are not added to `seen_bindings`.** Unlike boolean-only grounding where an action is immediately marked as "seen", numerically-pruned actions are kept out of the deduplication set. This allows them to be retried in later iterations when bounds may have widened.

3. **Inner numeric fixpoint.** After processing all new actions in an outer iteration, the grounder re-applies all accepted actions' numeric effects in a loop until bounds stabilize. This handles non-idempotent effects correctly (e.g., `increment(x)` adds `rate(x)` to `value(x)`, and if `rate(x)` widened since the action was first processed, `value(x)` must widen too).

4. **Permanent prune tracking.** A `numeric_pruned_bindings` map tracks which bindings were pruned. Entries are removed when a binding is later accepted. What remains at fixpoint is logged as permanently pruned actions.

5. **Directional widening.** Per-side expansion counters (`lower_expansion_count`, `upper_expansion_count`) ensure that only the side of the interval that has been moving gets widened to infinity. This preserves useful finite bounds on the stable side.

6. **Ceiling/floor freezing.** Before the fixpoint loop, `precompute_freezes()` performs a lifted analysis of all action schemas' effects to determine which numeric fluent schemas can have their upper or lower bound frozen (never widened). This composes with directional widening: frozen sides are protected from widening regardless of expansion count, while non-frozen sides still use the threshold. The analysis uses `evaluate_constant_expr_range()` — a lifted variant of `evaluate_interval()` that resolves state variables to per-schema ranges of constant fluents rather than per-ground-instance bounds.

### Source Files

| File | Purpose |
|------|---------|
| `interval.hpp` | `Interval` struct with arithmetic, convex union, overlap check |
| `numeric_bounds_index.hpp/.cpp` | Ground numeric fluent → interval map, directional widening, ceiling/floor freezing |
| `interval_evaluator.hpp/.cpp` | `evaluate_interval()` and `numeric_precondition_satisfiable()` |
| `reachability_grounder.cpp` | Integration: `collect_numeric_effects()`, inner fixpoint, prune tracking |
