# FDI-Style Smart Grounding via Numeric Abstraction

## Status: **Implemented and Validated** (March 2026)

---

## Summary

RantanPlan now uses the **FDI method** (Scala & Vallati, *"Effective Grounding for Hybrid Planning Problems represented in PDDL+"*) to prune unreachable ground actions before they are ever instantiated. This operates at the **lifted** level — before grounding — and is complementary to the C++ RPG pass that operates post-grounding.

The feature is **on by default** and can be disabled with `--naive-grounding`. It requires the optional `up-fast-downward` package; when that is absent, it falls back transparently to the naive UP `Grounder`.

---

## Motivation

UP's naive `Grounder` computes the full Cartesian product of type-compatible objects for each action parameter, then prunes using only static boolean fluent checks. No reachability analysis. For domains with high-arity actions and many objects, this creates a massive blowup — most ground actions are unreachable and will only be discarded later by the C++ RPG.

The key insight: **move the pruning upstream**, at the lifted level, before the combinatorial explosion ever materializes. This saves time and memory in grounding, protobuf serialization, C++ deserialization, and RPG processing.

---

## How It Works

### The Two-Problem Dance

Fast Downward **cannot handle numeric PDDL** (`(:functions ...)`, `increase`/`decrease`, numeric comparisons). The solution is a two-problem approach:

1. **Abstract** the numeric problem into a classical one (boolean proxy predicates preserving parameter linkage)
2. **Ground** the abstraction using FD's delete-relaxation reachability
3. **Extract** which `(action, param_tuple)` combinations FD found reachable
4. **Ground** the **original** numeric problem restricted to only those combinations

```
Original Numeric Problem
        │
        ├──► NumericAbstractor.abstract()
        │         │
        │         ▼
        │    Classical Problem (boolean proxy predicates)
        │         │
        │         ▼
        │    FastDownwardReachabilityGrounder.compile()
        │         │
        │         ▼
        │    Grounded Classical Problem
        │         │
        │         ▼
        │    _build_grounding_map()  [uses CompilerResult.map_back_action_instance()]
        │         │
        │         ▼
        │    grounding_actions_map: Dict[Action, List[Tuple[FNode, ...]]]
        │
        ├──► Grounder(grounding_actions_map=...).compile()
        │         │
        │         ▼
        │    Grounded Numeric Problem (full semantics, restricted bindings)
        │
        ▼
   C++ Backend (via protobuf)
```

### Abstraction Rules (`NumericAbstractor`)

| Original (Numeric) | Abstracted (Classical) |
|---|---|
| `(:functions (fuel ?a - aircraft))` | `(:predicates (fuel__num ?a - aircraft))` |
| `(>= (fuel ?a) (* (distance ?c1 ?c2) (slow-burn ?a)))` | `(and (fuel__num ?a) (distance__num ?c1 ?c2) (slow-burn__num ?a))` |
| `(decrease (fuel ?a) (distance ?c1 ?c2))` | `(fuel__num ?a) (distance__num ?c1 ?c2)` as add-effects |
| `(:init (= (fuel plane1) 100))` | `(:init (fuel__num plane1))` |

The abstracted problem has **strictly weaker preconditions** and **strictly more effects** than the original. This guarantees that every action reachable in the numeric domain is also reachable in the abstraction — a sound over-approximation. No valid grounding is ever missed.

### Soundness Chain

If ground action `move(rover1, wp3, wp7)` could appear in any valid plan for the original numeric problem, then:

1. **Numeric → Classical abstraction** only removes constraints → reachable in abstraction ✓
2. **FD delete-relaxation** only removes delete effects → still reachable ✓
3. Therefore FD keeps it and it enters the `grounding_actions_map` ✓

What gets pruned: actions unreachable **even under both relaxations** — e.g., `move(rover1, wp3, wp7)` where `rover1` can never reach `wp3` from the initial state in any relaxed execution.

### Relationship to C++ RPG Pass

| | FD Grounder (Python, pre-grounding) | C++ RPG (post-grounding) |
|---|---|---|
| **Stage** | Before grounding (lifted) | After grounding (grounded) |
| **Input** | Action schemas + objects | Full ground action set |
| **Bottleneck avoided** | Grounding explosion (memory + time) | Encoding unreachable actions (SAT variables) |
| **Memory** | Never materializes pruned actions | Must first load all actions |
| **Precision** | Sound over-approx (classical abstraction + delete-relaxation) | More precise (works on actual numeric problem; numeric RPG uses Z3) |

They are **complementary, not redundant**. FD prevents the explosion; the C++ RPG cleans up whatever FD's over-approximation lets through. After the RPG pass, both pipelines (naive and smart) converge to the same or very similar set of ground actions — the difference is in the **cost to get there**.

---

## Implementation

### Files Created

| File | LOC | Purpose |
|---|---|---|
| `rantanplan/numeric_abstractor.py` | 394 | `NumericAbstractor` class: transforms numeric UP `Problem` into classical `Problem` with boolean proxy predicates |
| `rantanplan/reachability_grounder.py` | 183 | `ReachabilityGrounder` class: orchestrates abstract → FD → map → ground pipeline |
| `test_grounding.py` | ~80 | Smoke tests for abstractor and grounder |
| `test_grounding_bench.py` | ~100 | Benchmark comparison of naive vs smart grounding |

### Files Modified

| File | Change |
|---|---|
| `rantanplan/planner_wrapper.py` | Added `naive_grounding` option to `__init__`; modified `_compile_problem()` Step 3 to use `ReachabilityGrounder` by default |
| `rantanplan/cli.py` | Added `--naive-grounding` CLI flag; passes through to planner params |
| `convert.py` | Added `--naive-grounding` flag; `compile_problem()` uses `ReachabilityGrounder` by default |
| `pyproject.toml` | Added `[project.optional-dependencies] grounding = ["up-fast-downward>=0.5"]` |

### Key Design Decisions Made During Implementation

1. **`CompilerResult.map_back_action_instance()` instead of name parsing**: The initial plan called for parsing grounded action names (`action_param1_param2`) which is fragile when action/object names contain underscores. The actual implementation uses `fd_result.map_back_action_instance(ActionInstance(grounded_action))` to robustly trace each grounded action back to its lifted action and parameters. This follows the same pattern used by UP's `TarskiGrounder`.

2. **Three-branch dispatch in `ReachabilityGrounder.ground()`**:
   - If `up-fast-downward` not installed → naive `Grounder()` with warning
   - If problem is purely classical (no numeric fluents) → `FastDownwardReachabilityGrounder` directly (no abstraction needed)
   - If problem has numeric features → full FDI pipeline (abstract → FD → map → ground)

3. **Exception handling with fallback**: If FD reachability fails for any reason (unexpected PDDL features in the abstraction, FD binary error, etc.), the grounder catches the exception and falls back to naive grounding rather than crashing.

4. **Lazy import of `up_fast_downward`**: The import is deferred to runtime inside `_fd_available()` and `ground()`, so the core package never fails if the optional dependency is absent.

### `NumericAbstractor` Details

The abstractor (~394 LOC) handles these cases:

- **Fluents**: Boolean fluents copied verbatim; numeric/real fluents replaced with boolean proxies named `{name}__num` with identical parameter signatures
- **Preconditions**: Recursive expression walking — boolean sub-trees copied as-is; numeric comparisons (LE, LT, EQUALS) replaced with conjunction of proxy applications for all fluent references in the sub-tree
- **Effects**: Boolean add/delete effects copied; numeric effects (increase/decrease/assign) converted to add-effects on proxy predicates for the affected fluent and all fluents referenced in the value expression
- **Initial state**: Boolean assignments copied; numeric assignments `(= (f obj1..objk) val)` → `(f__num obj1..objk) = True`
- **Goals**: Same transformation as preconditions
- **Edge cases**: 0-ary numeric fluents become 0-ary predicates; nested arithmetic extracts all fluent references; constants ignored; name collision guard appends `_0`, `_1`, etc.

### `ReachabilityGrounder` Details

The orchestrator (~183 LOC) provides:

- `ground(problem, compilation_kind) -> CompilerResult`: Main entry point
- `_build_grounding_map(original, grounded_abstracted, fd_result) -> Dict[Action, List[Tuple[FNode, ...]]]`: Maps FD results back to original problem using `map_back_action_instance()`
- `_naive_estimate(problem) -> int`: Cheap Cartesian-product upper bound for diagnostic logging
- Diagnostic output showing abstraction stats, FD reachability results, and reduction ratio

---

## Test Results

### Smoke Tests (`test_grounding.py`)

Using Zenotravel test domain:
- **Abstractor**: 10 fluents total, 8 numeric fluents abstracted to boolean proxies, all action schemas preserved with identical names and parameter counts
- **ReachabilityGrounder**: 37 reachable ground actions (matches naive grounder — all bindings happen to be reachable in this small instance)

### Benchmark Comparison (`test_grounding_bench.py`)

| Domain | Instance | Naive Actions | Smart Actions | Reduction | Grounding Time (Naive → Smart) |
|---|---|---|---|---|---|
| **Zenotravel** | test/problem.pddl | 37 | 37 | 1.0× | — |
| **Rover** | test/problem.pddl | 78 | 64 | 1.2× | — |
| **Rover** | bench/pfile16 | 1,059 | 683 | **1.6×** | 17.0s → 1.1s |
| **Hydropower** | test/problem.pddl | 5,355 | 146 | **36×** | (via quick test) |

### Full Regression (`python3 test.py --quick`)

**100/100 tests passed** across 4 domains (zenotravel, rover, gripper, hydropower) × 25 strategies. All plans validated by UP's `PlanValidator`. The smart grounder is the default in all test runs.

Notable observation from the test output:
- **Hydropower**: Naive estimate ~5,355 ground actions → FD reachability finds only 146 reachable → **36× reduction**. This is the biggest win, as hydropower has many high-arity actions with strongly constrained parameter interactions.
- **Gripper**: Purely classical domain — FD handles directly without abstraction (no numeric fluents).
- **Zenotravel/Rover (small test instances)**: Modest or no reduction since most parameter combinations are reachable in small instances.

---

## Usage

```bash
# Default behavior — smart grounding enabled
rantanplan -d domain.pddl -p problem.pddl --strategy forall-lazy-semantic-chain

# Disable smart grounding (use naive cross-product)
rantanplan -d domain.pddl -p problem.pddl --strategy seq --naive-grounding

# Convert with smart grounding
python3 convert.py -d domain.pddl -p problem.pddl -o problem.pb

# Convert with naive grounding
python3 convert.py -d domain.pddl -p problem.pddl -o problem.pb --naive-grounding

# Install the optional dependency
pip install up-fast-downward
# Or with the package:
pip install -e ".[grounding]"
```

---

## Theoretical Note: Why Both Stages Matter

One might ask: if the C++ RPG pass also does delete-relaxation reachability on grounded actions, doesn't it produce the same result?

**Yes, approximately.** After the RPG pass, both pipelines (naive and smart grounding) converge to the same or very similar set of surviving actions. The difference is **computational cost**:

- **Naive → RPG**: Materializes 5,355 Python `Action` objects → serializes all to protobuf → C++ deserializes all → RPG processes all → prunes to ~146
- **Smart → RPG**: FD finds 146 reachable at the lifted level → only 146 get instantiated → small protobuf → fast deserialization → RPG has little to prune

For larger instances (Rover pfile20 with ~370K naive ground actions), the naive path may **run out of memory or time** before the RPG gets its chance. The smart grounder prevents the explosion from ever happening.

The grounded Boolean RPG may be **slightly more precise** than FD on the abstraction, because:
- FD works on a classical abstraction (numeric information discarded)
- The Boolean RPG works on the actual boolean structure of the real grounded actions
- The Numeric RPG is even more precise (uses Z3 for interval arithmetic bounds)

So the two stages are complementary: FD handles the combinatorial explosion cheaply at the lifted level; the C++ RPG refines with greater precision at the grounded level.

---

## Decisions

- **`up-fast-downward` rather than custom pure-Python grounder**: Leverages FD's battle-tested Datalog-based reachability (~60K LOC of C++). The abstraction module is the only novel piece (~400 LOC Python).
- **`FastDownwardReachabilityGrounder` (not `FastDownwardGrounder`)**: Lighter, preserves action structure, doesn't introduce axioms.
- **`grounding_actions_map` bridge**: The FD grounder produces a grounded **abstracted** problem. We need the grounded **original numeric** problem. `grounding_actions_map` tells UP's `Grounder` to instantiate only the reachable parameter combinations on the original. This two-problem approach is fundamental.
- **Default ON with `--naive-grounding` opt-out**: Sound over-approximation — never misses a valid grounding. The flag is an escape hatch.
- **Optional dependency (GPLv3)**: `up-fast-downward` is GPL-licensed and ~50MB. Graceful fallback to naive grounding when absent.
- **Standalone module**: Simpler than UP `CompilerMixin`, called directly from `_compile_problem()`.

---

## Risks and Mitigations

| Risk | Mitigation | Status |
|---|---|---|
| FD rejects the abstracted problem | Catch exception, fall back to naive | **Implemented** — `try/except` in `ground()` |
| Action name parsing fragility | Used `CompilerResult.map_back_action_instance()` instead | **Resolved** — robust approach |
| Abstraction misses a parameter dependency | Comprehensive testing | **Validated** — 100/100 tests pass |
| `up-fast-downward` version incompatibility | Pinned `>=0.5` in pyproject.toml | **Done** |
| GPL license contamination | Optional dependency, lazy import | **Done** |

---

## Future Extensions

1. **Leverage C++ RPG for additional pruning**: After FDI grounding, the C++ backend's RPG (Boolean or Numeric) already removes unreachable actions post-grounding. The two techniques are complementary — FDI prunes before encoding, RPG prunes before solving.

2. **Custom pure-Python grounder fallback** — lightweight delete-relaxation forward-chaining (~400 LOC) for environments where installing `up-fast-downward` is impractical.

3. **Skip abstraction for small problems**: The `_naive_estimate()` function already computes `∑_action ∏_param |Objects(param.type)|` in O(actions × max_params) — essentially free. If the estimate is below a threshold (e.g., < 200), skip the FD subprocess entirely since naive grounding is already fast and the FD startup cost (~0.5s) dominates. This is a simple constant-time heuristic check before entering the FDI pipeline.

4. **Tighter abstraction** — preserve some numeric constraints in the abstraction (e.g., bounds on numeric fluents from initial state) for more precise reachability. The current abstraction is maximally permissive.
