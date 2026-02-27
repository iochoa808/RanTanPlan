# RantanPlan Next-Generation Infrastructure Plan

> Adopting key ideas from `planning_smt_spec.md` into the existing codebase — incrementally, without touching the solver layer, propagators, or parallelism system.

---

## Executive Summary

### What We're Changing
Four infrastructure improvements drawn from the proposed `planning_smt_spec.md` design:

1. **Expression Interning** (ExprPool + ExprID) — eliminate `to_string()`-based hashing — **COMPLETE**
2. **Problem Immutability** — replace in-place mutation with functional transformations
3. **Pass System** — composable, reorderable preprocessing pipeline
4. **Lazy Analysis Facade** (TaskAnalysis) — shared, on-demand derived data

### What We're NOT Changing
- Z3 solver integration (no SMT abstraction layer)
- Propagator system (Z3 user propagators remain Z3-specific)
- Parallelism semantics and interference analysis architecture
- Strategy configuration system
- Python frontend and UP integration

### Why Not a Full Rewrite?
The spec covers ~30% of what RantanPlan does. The other 70% — parallel semantics, 4 interference analysis variants, 5 propagator implementations, 25+ strategy configurations, incremental BMC — represents years of battle-tested research code. A full rewrite would take 6-9 months and risk subtle SAT encoding regressions. The incremental approach never leaves the system in a broken state.

---

## Phase 1: Expression Interning — COMPLETE

Fully implemented and tested (100/100 quick tests pass). The old `Expression` class, its visitor hierarchy, and all bridging infrastructure have been completely removed.

### What Was Built

**New files:** `expr_pool.hpp`, `expr_pool.cpp`, `expr_enums.hpp`

**ExprID:** Strong typedef over `int32_t`. Value `-1` = null/invalid. O(1) equality and hashing.

**ExprPool:** Append-only, structurally interning pool with canonical-key-based deduplication. Rich query API: kind/operator checks, children access, payload access, semantic helpers.

**ExprKind / ExprOperator:** Standalone enums in `expr_enums.hpp` with utility functions (`string_to_operator`, `operator_to_string`, `is_arithmetic_operator`, etc.).

**Problem:**
- `pool()`, `pool_ptr()` — access shared ExprPool
- `intern_from_protobuf(const pb::Expression&)` — direct protobuf → ExprID interning (no intermediate Expression)
- `is_bool_type(ExprID)`, `is_numeric_type(ExprID)`, `type_for_id(ExprID)` — O(1) type queries

**ExprID fields on core types** (populated during protobuf deserialization):
- `Action::precondition_id()`, `has_precondition()`
- `EffectExpression::fluent_id()`, `value_id()`, `condition_id()`, `is_conditional()`
- `Goal::goal_id()`
- `Assignment::fluent_id()`, `value_id()`

**ExprID-based Z3 conversion:**
- `GroundedEncodingVisitor::convert_from_pool(ExprID, int timestep)` — walks ExprNode tree directly
- `LiftedEncodingVisitor::convert_from_pool(ExprID, int timestep)` — same for lifted/symmetry encoding
- `BaseEncoder::convert_expr_id_to_z3(ExprID, int timestep)` — sole virtual interface (no Expression variant)

### What Was Removed

- **Deleted files**: `expression.hpp`, `expression.cpp`, `expression_visitor.hpp`, `expression_visitor.cpp`, `print_visitor.hpp`, `variable_collector.hpp`
- **Removed from Problem**: `expr_to_id_`, `id_to_expr_` caches, `intern_expression()`, `expr_id()`, `expression_for_id()`, `intern_all_expressions()`, `find_grounded_fluent_index(const Expression&)`
- **Removed from BaseEncoder**: `convert_expression_to_z3(const Expression&, int)` pure virtual
- **Removed from visitors**: Both `GroundedEncodingVisitor` and `LiftedEncodingVisitor` no longer inherit from `BaseExpressionVisitor`; all `visit_*` methods deleted
- **Removed from Fluent**: `optional<Expression> default_value_` member and accessors
- **Removed from ARPG**: old `evaluate_expression(const Expression&)` and `satisfies_condition(const Expression&)` methods

### All 10 Expression-keyed containers migrated to ExprID keys
Problem, GroundedEncoder, InterferenceAnalysis (9 sets), ChainedGroundedEncoder, R2EGroundedEncoder (4 maps), FluentCollector, FluentPolarityCollector, NumericRelaxedPlanningGraph, AchieversAnalysis, DecisionHeuristicPropagator, SMTSymmetryChecker.

---

## Phase 2: Problem Immutability

### Goal
Make `Problem` immutable after construction. Replace in-place mutation with functional transformation.

### Key Insight
Only **2 call sites** mutate Problem (both RPG action removal via `remove_action(size_t)`). **146+** consumers already take `const Problem&`.

### Design

**New method on Problem:**
```cpp
/// Returns a new Problem with the specified actions removed.
/// Action IDs in the new Problem are contiguous [0..N-1].
/// ExprPool is shared (not copied) via shared_ptr.
Problem without_actions(const std::vector<size_t>& removed_indices) const;
```

Implementation:
1. Copy all Problem data except actions
2. Build new actions vector skipping removed indices
3. Re-index action IDs to be contiguous
4. Rebuild internal index maps (`action_name_to_index_`, etc.)
5. Share ExprPool via `shared_ptr` (already a `shared_ptr` on Problem)

**RPG changes:**

Both `RelaxedPlanningGraph` and `NumericRelaxedPlanningGraph` have **identical** `remove_unreachable_actions()` implementations (exact code duplication). Both:
1. Call `get_removable_actions()` to get a list of `const Action*`
2. Linear-scan to find indices (O(n²) for n removable actions)
3. Sort indices descending, remove from highest to lowest

| Before | After |
|--------|-------|
| `RelaxedPlanningGraph(Problem& problem)` | `RelaxedPlanningGraph(const Problem& problem)` |
| `remove_unreachable_actions() -> size_t` (mutates) | `get_removable_action_indices() const -> vector<size_t>` |

Same change for `NumericRelaxedPlanningGraph`. The duplicated `remove_unreachable_actions()` implementation is deleted from both classes — it becomes a 3-line snippet in `main.cpp`.

**main.cpp transformation:**
```cpp
// BEFORE (current):
RelaxedPlanningGraph rpg(planning_problem);  // mutable ref
rpg.build();
rpg.remove_unreachable_actions();            // mutates planning_problem

// AFTER:
RelaxedPlanningGraph rpg(planning_problem);  // const ref
rpg.build();
auto removed = rpg.get_removable_action_indices();
if (!removed.empty()) {
    planning_problem = planning_problem.without_actions(removed);
}
```

**Deprecate remaining mutation methods:**
After migration, remove `remove_action(size_t)`. The only construction path becomes the protobuf constructor + `without_actions()`.

### Files Modified
- `problem.hpp`, `problem.cpp` — add `without_actions()`, remove `remove_action()`
- `relaxed_planning_graph.hpp/.cpp` — `const Problem&`, replace `remove_unreachable_actions()` with `get_removable_action_indices()`
- `numeric_relaxed_planning_graph.hpp/.cpp` — same
- `main.cpp` — update pipeline

### Testing
- `python test.py --quick` — fast regression
- `python test.py` — full regression (all domains, all strategies)

---

## Phase 3: Pass System

### Goal
Replace procedural preprocessing logic in `main.cpp` with a composable, extensible pass pipeline.

### New Components

**Files:**
- `rantanplan/cpp/src/passes/pass.hpp`
- `rantanplan/cpp/src/passes/pipeline.hpp`
- `rantanplan/cpp/src/passes/unreachable_action_removal_pass.hpp/.cpp`

**Pass interface:**
```cpp
class Pass {
public:
    virtual Problem apply(const Problem& in) const = 0;
    virtual std::string name() const = 0;
    virtual ~Pass() = default;
};

/// Chain passes left-to-right. Logs each pass name + timing.
Problem run_pipeline(Problem initial, const std::vector<const Pass*>& passes);
```

**First concrete pass — UnreachableActionRemovalPass:**
```cpp
class UnreachableActionRemovalPass : public Pass {
public:
    enum class Mode { BooleanRPG, NumericRPG };
    explicit UnreachableActionRemovalPass(Mode mode, z3::context* ctx = nullptr);

    Problem apply(const Problem& in) const override;
    std::string name() const override { return "unreachable-action-removal"; }

    /// Available after apply(): RPG-derived lower bound on plan length
    int lower_bound() const { return lower_bound_; }

private:
    Mode mode_;
    z3::context* ctx_;           // needed for NumericRPG only
    mutable int lower_bound_ = 0;
};
```

**main.cpp pipeline (replaces procedural RPG logic):**
```cpp
std::vector<const Pass*> passes;

UnreachableActionRemovalPass action_removal_pass(
    config.use_numeric_rpg
        ? UnreachableActionRemovalPass::Mode::NumericRPG
        : UnreachableActionRemovalPass::Mode::BooleanRPG,
    &ctx
);

if (!config.disable_action_removal) {
    passes.push_back(&action_removal_pass);
}
// Future passes: static fluent removal, numeric normalization, etc.

planning_problem = run_pipeline(std::move(planning_problem), passes);
int start_timestep = action_removal_pass.lower_bound();
```

### Files Modified
- New: `passes/pass.hpp`, `passes/pipeline.hpp`, `passes/unreachable_action_removal_pass.hpp/.cpp`
- `main.cpp` — replace procedural RPG logic with pipeline
- `CMakeLists.txt` — add new source files

### Testing
- `python test.py --quick` — fast regression
- `python test.py` — verify identical behavior across all strategies
- Test with `--no-action-removal` — pipeline runs with empty pass list

---

## Phase 4: Lazy Analysis Facade (TaskAnalysis)

### Goal
Provide a single access point for derived analysis data, computed lazily and shared across consumers. Eliminate duplicate computation of action-fluent relationships.

### Design

**Files:**
- `rantanplan/cpp/src/analysis/task_analysis.hpp/.cpp`

**TaskAnalysis** (computes and caches both views):
```cpp
class TaskAnalysis {
public:
    explicit TaskAnalysis(const Problem& problem);

    /// Per-action analysis (same shape as InterferenceAnalysis::ActionAnalysis).
    /// Computed lazily on first access.
    struct ActionSummary {
        std::unordered_set<ExprID> positive_boolean_preconditions;
        std::unordered_set<ExprID> negative_boolean_preconditions;
        std::unordered_set<ExprID> numeric_preconditions;
        std::unordered_set<ExprID> positive_boolean_effects;
        std::unordered_set<ExprID> negative_boolean_effects;
        std::unordered_set<ExprID> numeric_effects;
        std::unordered_set<ExprID> all_effects;
        std::unordered_set<ExprID> conditional_effect_fluents;
        std::unordered_set<ExprID> numeric_effect_dependencies;
    };

    /// Per-fluent inverse index. Computed lazily on first access.
    struct InverseIndex {
        std::unordered_map<ExprID, std::vector<int>> producers;
        std::unordered_map<ExprID, std::vector<int>> deleters;
        std::unordered_map<ExprID, std::vector<int>> num_writers;
        std::unordered_map<ExprID, std::vector<int>> pos_requirers;
        std::unordered_map<ExprID, std::vector<int>> neg_requirers;
    };

    const ActionSummary& action_summary(int action_id) const;
    const InverseIndex& inverse_index() const;

private:
    const Problem& problem_;
    mutable std::vector<ActionSummary> summaries_;  // indexed by action id
    mutable std::optional<InverseIndex> inverse_;
    mutable bool summaries_built_ = false;

    void build_summaries() const;
    void build_inverse_index() const;
};
```

**Integration with interference analysis:**
- `InterferenceAnalysis::ActionAnalysis` typedef becomes `TaskAnalysis::ActionSummary`
- `InterferenceAnalysis::analyze_all_actions()` can delegate to `TaskAnalysis` instead of re-computing
- This is a soft dependency — interference analysis still works standalone without TaskAnalysis

### Files Modified
- New: `analysis/task_analysis.hpp/.cpp`
- `interference_analysis.hpp/.cpp` — optionally accept `TaskAnalysis` reference
- `achievers_analysis.hpp/.cpp` — optionally use `InverseIndex` instead of recomputing
- `CMakeLists.txt` — add new source files

### Testing
- `python test.py --quick` — fast regression
- `python test.py` — full regression

---

## Phase Dependencies

```
Phase 1 (ExprPool + ExprID) ── COMPLETE
    │
    ▼
Phase 2 (Problem Immutability)  ← shares ExprPool via shared_ptr
    │
    ▼
Phase 3 (Pass System)           ← requires without_actions() from Phase 2
    │
    ▼
Phase 4 (TaskAnalysis)          ← uses ExprID keys from Phase 1
```

Each phase is independently testable and mergeable. Phases 3 and 4 could theoretically be swapped.

---

## Verification Plan

After all phases are complete:

1. **Regression:** `python test.py` — all domains, all strategies pass
2. **Smoke:** `python test.py --quick` — fast subset
3. **Performance:** `python benchmark.py --jobs 4 --timeout 60` — should see improvement from interning, no regressions
4. **Manual:** Run problems with `--verbose`, compare plan output to pre-migration
5. **Config variants:** Test with `--no-action-removal`, `--boolean-rpg`, `--numeric-rpg`
6. **All strategies:** Verify `seq`, `forall-lazy-semantic-chain`, `exists-lazy-semantic-chain`, `r2e`, `dec` all produce valid plans

---

## What This Enables (Future Work)

With the infrastructure in place, new passes become trivial to add:
- **StaticFluentRemovalPass** — remove fluents that never change
- **NumericNormalizationPass** — classify numeric effects (constant, linear, nonlinear, monotone)
- **MutexDetectionPass** — compute mutex groups for tighter encodings
- **InvariantSynthesisPass** — discover and inject state invariants
- **RelevanceAnalysisPass** — remove fluents/actions irrelevant to the goal

The lazy TaskAnalysis facade can grow to include:
- RPG and NumericRPG (moved from passes to analysis)
- ARPG interval analysis
- Action ordering heuristics
- Shared across interference analysis, achievers analysis, and future components
