# Plan: C++ Reachability Grounder

## Status: **Planned**

---

## Goal

Move grounding from the Python side into the C++ backend, implementing a delete-relaxation reachability grounder that:

1. Accepts both **lifted** (ungrounded) and **grounded** problems via protobuf
2. Auto-detects which case it's handling
3. If lifted and the strategy requires grounded actions, runs a join-based reachability grounder
4. Unifies grounding with the existing RPG pass into a single preprocessing step

This eliminates the `up-fast-downward` dependency, the `NumericAbstractor`, the subprocess overhead, and the GPL concern — while being faster (C++ vs Python) and architecturally cleaner (grounding lives where encoding lives).

---

## Context: Current State

### Uncommitted Python-Side Grounder

There are uncommitted changes implementing a Python-side FDI smart grounder:

| File | Status | Description |
|---|---|---|
| `rantanplan/numeric_abstractor.py` | New (untracked) | Numeric-to-classical abstraction for FD |
| `rantanplan/reachability_grounder.py` | New (untracked) | Orchestrator: abstract → FD → map → ground |
| `rantanplan/planner_wrapper.py` | Modified | Uses `ReachabilityGrounder` by default |
| `rantanplan/cli.py` | Modified | Added `--naive-grounding` flag |
| `convert.py` | Modified | Uses smart grounding by default |
| `pyproject.toml` | Modified | Added `up-fast-downward` optional dep |
| `test_grounding.py` | New (untracked) | Smoke tests |
| `test_grounding_bench.py` | New (untracked) | Benchmark comparison |
| `BETTER_GROUNDER.md` | New (untracked) | Design document for the Python-side approach |

These changes work and all tests pass (100/100 on `test.py --quick`). However, the approach has structural limitations: it requires an external GPL dependency (`up-fast-downward`), spawns a subprocess with ~0.5s startup cost, and requires a two-problem dance to handle numeric PDDL (FD only understands classical problems).

### What the C++ Side Already Has

The C++ backend already has ~90% of the infrastructure needed:

- **`Problem` class**: Stores actions with `parameters_` (typed `Parameter` vector), fluent schemas with parameters, type hierarchy with `is_subtype_of()`, and a complete object set
- **`ExprPool`**: Interned expression trees with `ExprKind::PARAMETER` nodes that represent unbound action parameters
- **Protobuf schema**: `Action` messages already have `repeated Parameter parameters`; `Expression` messages support `ExpressionKind::PARAMETER` atoms
- **Boolean RPG** (`RelaxedPlanningGraph`): Already implements delete-relaxation fixpoint iteration — but on grounded actions. The lifted grounder is the same algorithm operating on lifted actions with join-based matching
- **Pipeline system** (`Pass` / `PipelineResult`): Clean pass infrastructure for preprocessing; the grounder is naturally a new pass

The key piece that is missing: a **join-based binding matcher** that, given a lifted action's preconditions and the current set of reachable facts, finds all type-consistent parameter substitutions where preconditions hold.

---

## Architecture Overview

```
                    ┌──────────────────┐
                    │  Python Frontend  │
                    │                  │
                    │  PDDL → UP Model │
                    │  QuantifiersRemover │
                    │  CNFConditionCompiler │
                    │  (NO grounding)  │
                    │                  │
                    │  ProtobufWriter  │
                    │        │         │
                    └────────┼─────────┘
                             │ protobuf (lifted or grounded)
                             ▼
              ┌──────────────────────────────┐
              │         C++ Backend          │
              │                              │
              │  Problem(pb)                 │ ← auto-detects lifted/grounded
              │        │                     │
              │        ▼                     │
              │  ┌─────────────┐             │
              │  │  Grounding  │ (if lifted) │  ← new GroundingPass
              │  │    Pass     │             │    join-based reachability
              │  └──────┬──────┘             │    + lower bound + action removal
              │         │                    │    SUBSUMES Boolean RPG
              │         ▼                    │
              │  ┌─────────────┐             │
              │  │ Numeric RPG │ (optional)  │  ← existing (numeric pruning only)
              │  │    Pass     │             │    NOT needed if problem is
              │  └──────┬──────┘             │    purely classical
              │         │                    │
              │         ▼                    │
              │  ┌─────────────┐             │
              │  │  Symmetry   │             │
              │  │    Pass     │             │
              │  └──────┬──────┘             │
              │         │                    │
              │         ▼                    │
              │  ┌─────────────┐             │
              │  │   Encoder   │             │
              │  │  + Solver   │             │
              │  └─────────────┘             │
              └──────────────────────────────┘
```

**Why the Boolean RPG is subsumed**: The grounding pass IS a Boolean RPG — it performs the same delete-relaxation fixpoint over boolean facts, layer by layer. The only difference is that it operates on lifted action schemas (finding bindings via join-based matching) rather than pre-grounded actions. At fixpoint, the set of reachable ground actions is identical to what the Boolean RPG would compute. The grounding pass also computes the same lower bound (first layer where all goals appear). Running the Boolean RPG after grounding would be a no-op — it would reach the same fixpoint instantly since all surviving actions are already reachable.

The Numeric RPG is NOT subsumed because it does something fundamentally different: it uses Z3 to compute numeric bounds on fluent values, which allows it to prune actions with unsatisfiable numeric preconditions. The grounding pass skips numeric preconditions entirely (assumes them satisfiable).

### Dual-Path Problem Flow

When the C++ backend receives a problem, it auto-detects whether it's grounded or lifted:

- **Grounded** (all actions have 0 parameters): Skip grounding pass. Run BooleanRPGPass or NumericRPGPass as today. This preserves the ability to use any future UP-side grounder.
- **Lifted** (any action has parameters): Run the `GroundingPass` (which subsumes the Boolean RPG). Optionally run `NumericRPGPass` for additional numeric pruning. Then proceed to symmetry/encoding.

---

## Detailed Steps

### Step 1: Python Side — Make Grounding Optional

**Files**: `rantanplan/planner_wrapper.py`, `rantanplan/cli.py`, `convert.py`

Add a `--cpp-grounding` flag (default: off initially, later becomes default). When enabled:

- Skip Step 3 (grounding) in `_compile_problem()` entirely
- `ProtobufWriter.convert()` serializes the **lifted** problem (after quantifier removal and optional CNF normalization)
- The C++ backend receives actions with parameters and `ExprKind::PARAMETER` nodes in their expression trees

The existing `--naive-grounding` and default smart-grounding paths remain for backward compatibility. The flags become:

| Flag | Grounding Location | Method |
|---|---|---|
| (default, no flag) | Python | FDI smart grounder (`ReachabilityGrounder`) |
| `--naive-grounding` | Python | UP's naive `Grounder` (cross-product) |
| `--cpp-grounding` | C++ | New `GroundingPass` (this plan) |

Once the C++ grounder is validated, `--cpp-grounding` becomes the default and the Python-side grounding code path is deprecated.

**Note**: `_initialize_fluents()` currently runs before compilation and sets defaults for uninitialized fluents on the lifted problem. This should continue to work — it iterates fluent schemas × objects, which is the same regardless of grounding. If the C++ side is grounding, it can handle missing initial values by defaulting to `false` for booleans and `0` for numerics (as it already effectively does via `ExprPool` constants).

### Step 2: C++ Auto-Detection of Grounded vs Lifted

**File**: `problem/problem.hpp`, `problem/problem.cpp`

Add a method to `Problem`:

```cpp
/// Returns true if the problem is already grounded (all actions have 0 parameters).
/// A problem is considered grounded when every action has an empty parameter list.
/// This is the case when the Python frontend has already grounded the problem,
/// or when the problem was specified with only ground actions.
bool is_grounded() const;
```

Implementation:

```cpp
bool Problem::is_grounded() const {
    // A problem is grounded if every action has zero parameters.
    // When Python grounds the problem, it produces actions like "move_city0_city1"
    // with no parameters. When Python sends a lifted problem, actions have parameters
    // like "move" with parameters [?from - location, ?to - location].
    for (const auto& action : actions_) {
        if (!action.parameters().empty()) return false;
    }
    return true;
}
```

### Step 3: Expression Substitution Engine

**New files**: `problem/substitution.hpp`, `problem/substitution.cpp`

This is the core utility: given an ExprID tree containing `PARAMETER` nodes, produce a new ExprID tree with those parameters replaced by concrete object constants.

```cpp
/// A substitution maps parameter names to object ExprIDs.
/// Example: {"?from" -> ExprID(city0_constant), "?to" -> ExprID(city1_constant)}
using Substitution = std::unordered_map<std::string, ExprID>;

/// Recursively walk an ExprID tree and replace every PARAMETER node whose name
/// appears in `subst` with the corresponding ExprID. The result is interned
/// into the pool, so structurally identical substitutions share the same ExprID.
///
/// This is the fundamental operation for grounding: it turns a lifted expression
/// like (at ?obj ?loc) into a ground expression like (at robot1 city0).
///
/// How it works:
///   - Leaf PARAMETER node with name in subst → return subst[name]
///   - Leaf node (CONSTANT, FLUENT_SYMBOL, etc.) → return as-is (no children to recurse)
///   - Internal node (AND, OR, STATE_VARIABLE, etc.) → recurse on all children,
///     then intern a new node with the substituted children. If nothing changed,
///     the ExprPool returns the original ExprID (structural sharing).
///
/// Performance: O(tree_size) per substitution. The ExprPool's interning ensures
/// that repeated substitutions producing identical trees are deduplicated.
ExprID substitute(ExprPool& pool, ExprID expr, const Substitution& subst);
```

**Estimated LOC**: ~60-80

This is a straightforward recursive tree walk. The ExprPool's interning handles deduplication automatically — if two different actions produce the same ground fluent application `(at robot1 city0)`, they will share the same ExprID.

### Step 4: Fact Index

**New files**: `grounding/fact_index.hpp`, `grounding/fact_index.cpp`

A data structure that holds the set of reachable ground facts (boolean fluent applications) and supports efficient lookup for join-based matching.

```cpp
/// FactIndex stores the set of ground boolean facts known to be reachable.
///
/// It serves two purposes in the grounder:
/// 1. MEMBERSHIP TEST: "Is (at robot1 city0) reachable?" — used to check
///    whether an action's preconditions are satisfied under a given substitution.
/// 2. EXTENSION QUERY: "What objects x make (at robot1 x) reachable?" — used by
///    the join-based matcher to find substitutions that satisfy a precondition.
///
/// Facts are indexed by fluent schema (the fluent name/id), so looking up all
/// facts for a given fluent is O(1) in the number of distinct fluents.
///
/// Inside each fluent's bucket, facts are stored as vectors of object IDs
/// (the grounded arguments). For a fluent `at(obj, loc)`, a fact might be
/// stored as [3, 7] meaning object #3 at location #7.
///
/// Design note: We store object indices rather than ExprIDs for the arguments
/// because the join matcher needs to compare and bind individual argument
/// positions, not entire expressions.
class FactIndex {
public:
    explicit FactIndex(const Problem& problem);

    /// Seed the index with all boolean facts from the problem's initial state.
    /// Numeric initial values are ignored — they don't constrain reachability
    /// in the delete-relaxation (any numeric condition is assumed satisfiable).
    void initialize_from_initial_state();

    /// Add a ground fact. Returns true if the fact was new (not previously known).
    /// This is called when an action's add-effects produce new facts.
    bool add_fact(int fluent_schema_id, const std::vector<int>& object_ids);

    /// Check if a specific ground fact exists.
    bool contains(int fluent_schema_id, const std::vector<int>& object_ids) const;

    /// Get all known ground tuples for a given fluent schema.
    /// Returns a reference to the set of known argument tuples.
    /// Used by the join matcher to enumerate candidate bindings.
    ///
    /// Example: for fluent "at" (schema id=2), might return:
    ///   {{0, 3}, {0, 5}, {1, 3}} meaning at(obj0,loc3), at(obj0,loc5), at(obj1,loc3)
    const std::vector<std::vector<int>>& get_facts(int fluent_schema_id) const;

    /// Total number of known facts across all fluents.
    size_t total_fact_count() const;

private:
    const Problem& problem_;

    // Primary storage: fluent_schema_id → list of ground argument tuples.
    // Each tuple is a vector of object indices (position in Problem::objects()).
    std::vector<std::vector<std::vector<int>>> facts_by_fluent_;

    // Fast membership check: fluent_schema_id → set of hashed tuples.
    // We hash the argument tuple for O(1) lookup.
    std::vector<std::unordered_set<uint64_t>> fact_hashes_;

    /// Hash a ground argument tuple for fast lookup.
    static uint64_t hash_tuple(const std::vector<int>& object_ids);
};
```

**Estimated LOC**: ~120-150

**Design rationale**: The two-level structure (by fluent schema, then by argument tuple) is critical for the join matcher. When matching a precondition like `(at ?obj ?loc)`, the matcher first looks up fluent "at" to get all known tuples, then iterates those tuples to find object bindings consistent with any already-bound parameters. Without the fluent-level index, the matcher would have to scan all facts of all fluents for every precondition.

### Step 5: Join-Based Binding Matcher

**New files**: `grounding/binding_matcher.hpp`, `grounding/binding_matcher.cpp`

This is the core algorithmic piece. Given a lifted action and the current `FactIndex`, it finds all type-consistent parameter substitutions where every boolean precondition is satisfied.

```cpp
/// A partial binding maps some (not necessarily all) of an action's parameters
/// to concrete objects. As preconditions are matched, the binding grows until
/// all parameters are bound.
///
/// Example for action move(?v - vehicle, ?from - loc, ?to - loc):
///   {} → match "at(?v, ?from)" → {?v=truck1, ?from=city0}
///        → match "road(?from, ?to)" → {?v=truck1, ?from=city0, ?to=city1}
using PartialBinding = std::unordered_map<int, int>;  // param_index → object_index

/// BindingMatcher finds all complete, type-consistent parameter bindings for a
/// lifted action such that every boolean precondition holds in the current
/// FactIndex.
///
/// THE KEY ALGORITHM — JOIN-BASED MATCHING
/// ========================================
///
/// Instead of trying all |Objects(type₁)| × ... × |Objects(typeₖ)| combinations
/// (which is what naive grounding does), we treat precondition matching like a
/// database join:
///
/// 1. EXTRACT ATOMS: From the action's precondition ExprID tree, extract all
///    "positive boolean fluent applications" — these are the join conditions.
///    Example: for precond "and(at(?v,?from), road(?from,?to), not(visited(?to)))",
///    we extract: [at(?v,?from), road(?from,?to), visited(?to)].
///    Negated atoms are still extracted (they constrain reachability).
///
/// 2. ORDER BY SELECTIVITY: Sort atoms so the most constraining one comes first.
///    "Most constraining" = fewest matching tuples in the FactIndex, weighted by
///    how many NEW parameters the atom would bind.
///    Example: if FactIndex has 3 "at" facts and 50 "road" facts, process "at"
///    first — it produces only 3 candidate partial bindings vs 50.
///
/// 3. EXTEND INCREMENTALLY: Start with an empty binding {}. For the first atom,
///    look up all matching tuples in the FactIndex and create one partial binding
///    per match. For each subsequent atom, try to extend each existing partial
///    binding with new matches. Discard any partial binding that can't be extended.
///
///    This is essentially a multi-way hash join, processing one relation at a time.
///
/// 4. TYPE FILTER: When extending a binding, check that each proposed object
///    assignment is type-consistent with the parameter's declared type.
///
/// 5. NUMERIC PRECONDITIONS: Any precondition that involves numeric fluents or
///    arithmetic (comparisons like >=, <=, +, -, etc.) is SKIPPED. We cannot
///    evaluate these without actual numeric values, so we assume they're always
///    satisfiable. This is the same over-approximation as the delete-relaxation
///    — it means we may generate some ground actions whose numeric preconditions
///    can never be satisfied, but we never miss any that could be.
///
/// WHY THIS IS FAST
/// ================
///
/// For an action move(?v, ?from, ?to) with 100 vehicles and 100 locations:
///   - Naive: tries 100 × 100 × 100 = 1,000,000 combinations
///   - Join: if only 5 vehicles are ever "at" anything, starts with 5 partial
///     bindings, each extended by the "road" facts connecting that location.
///     Might produce ~500 complete bindings instead of 1,000,000 trials.
///
/// The key invariant: we never create a partial binding that can't possibly
/// lead to a complete valid binding (because each step only extends with
/// actually-reachable facts).
class BindingMatcher {
public:
    explicit BindingMatcher(const Problem& problem, const FactIndex& facts);

    /// Find all complete parameter bindings for a lifted action.
    ///
    /// @param action The lifted action (has non-empty parameters())
    /// @return Vector of complete bindings (param_index → object_index maps),
    ///         each representing one reachable ground instantiation.
    std::vector<PartialBinding> find_bindings(const Action& action) const;

private:
    const Problem& problem_;
    const FactIndex& facts_;

    /// Extract all boolean fluent atoms from a precondition ExprID tree.
    /// Walks AND/OR/NOT structure; collects STATE_VARIABLE leaves.
    /// Skips numeric sub-trees (comparisons, arithmetic).
    struct PrecondAtom {
        int fluent_schema_id;           // Which fluent this atom references
        std::vector<int> param_indices; // For each fluent argument: which action
                                        // parameter it references (-1 if constant)
        std::vector<int> constant_object_ids; // For constant args: the object index
                                              // (-1 if parameterized)
        bool negated;                   // Whether this appears under NOT
    };

    std::vector<PrecondAtom> extract_atoms(const Action& action) const;

    /// Order atoms by selectivity: fewest matching facts × most new bindings first.
    void order_by_selectivity(std::vector<PrecondAtom>& atoms,
                              const std::unordered_set<int>& already_bound) const;

    /// Extend a set of partial bindings with matches for one more atom.
    std::vector<PartialBinding> extend_bindings(
        const std::vector<PartialBinding>& current,
        const PrecondAtom& atom,
        const Action& action) const;

    /// Check type consistency: is object at index `obj_idx` compatible with
    /// the declared type of parameter at index `param_idx`?
    bool is_type_compatible(const Action& action, int param_idx, int obj_idx) const;

    /// Build type-compatible object lists per parameter (cached per action).
    std::vector<std::vector<int>> get_type_compatible_objects(const Action& action) const;
};
```

**Estimated LOC**: ~250-300

**The selectivity heuristic** is the difference between "fast" and "still exponential in the worst case". A greedy heuristic works well in practice:

```
score(atom) = num_matching_facts(atom.fluent_id) / max(1, num_new_params(atom))
```

Process atoms with the lowest score first. This means we start with the most constrained atom (fewest matches per new variable) and progressively narrow the candidate set.

### Step 6: Lifted Reachability Grounder

**New files**: `grounding/reachability_grounder.hpp`, `grounding/reachability_grounder.cpp`

The main grounder class that ties together `FactIndex`, `BindingMatcher`, and `substitute()` into a fixpoint loop.

```cpp
/// ReachabilityGrounder performs delete-relaxation reachability analysis on a
/// lifted planning problem, producing a grounded problem containing only those
/// ground actions that are reachable from the initial state.
///
/// THE ALGORITHM
/// =============
///
/// This is the same algorithm as a Relaxed Planning Graph (RPG), but operating
/// on LIFTED actions instead of grounded ones. The combination of grounding and
/// reachability analysis in a single pass means we never instantiate ground
/// actions that are unreachable — the key advantage over "ground everything,
/// then prune".
///
///   1. Initialize the FactIndex with all boolean facts from the initial state.
///
///   2. Repeat until no new facts are discovered (fixpoint):
///      a. For each lifted action schema:
///         - Use BindingMatcher to find all parameter bindings where the
///           boolean preconditions hold in the current FactIndex.
///         - For each NEW binding (not seen before):
///           • Record it as a reachable ground action.
///           • Apply all add-effects (substituted) to the FactIndex.
///           • (Delete effects are ignored — this is delete-relaxation.)
///      b. If no new facts were added in this iteration, stop.
///
///   3. Build a grounded Problem:
///      - For each reachable (action_schema, binding) pair, construct a ground
///        Action with name "schema_obj1_obj2_...", empty parameters, and
///        fully substituted precondition/effect ExprIDs.
///      - Reassign contiguous action IDs [0..N-1].
///      - Recompute grounded_fluents_ from initial state + all reached facts.
///
/// PROPERTIES
/// ==========
///
/// - SOUND: Never removes a ground action that could appear in a valid plan.
///   Proof: The delete-relaxation is an over-approximation of the true
///   reachable set. Any fact reachable with delete effects is also reachable
///   without them. Any action applicable in the real state space has all its
///   boolean preconditions satisfied in the relaxed FactIndex.
///
/// - TERMINATING: The fact set is bounded by |fluents| × |objects|^max_arity.
///   Each iteration adds at least one new fact (or terminates). Therefore the
///   loop runs at most |fluents| × |objects|^max_arity iterations.
///
/// - COMPLETE for grounding: If the problem has a solution, all actions in
///   that solution will be in the grounded output.
///
/// NUMERIC HANDLING
/// ================
///
/// Numeric preconditions are skipped during matching (assumed always satisfiable).
/// Numeric effects are included in the ground actions but don't add boolean
/// facts. This is the same over-approximation as the Python-side FDI abstraction
/// (replace numeric constraints with `true`), but done implicitly without
/// creating a separate classical problem.
///
/// After grounding, the existing Numeric RPG pass can further prune actions
/// whose numeric preconditions are infeasible. The Boolean RPG is subsumed
/// (this grounder already computes the same delete-relaxation fixpoint).
class ReachabilityGrounder {
public:
    explicit ReachabilityGrounder(const Problem& problem);

    /// Run the reachability grounding algorithm.
    /// Returns a new fully-grounded Problem with only reachable actions.
    Problem ground();

    /// Get the RPG lower bound (max layer at which a goal fact first appears).
    /// Only valid after ground() has been called.
    int get_lower_bound() const;

    /// Get statistics about the grounding process.
    struct Stats {
        int num_lifted_schemas;       // Number of lifted action schemas
        int num_ground_actions;       // Number of reachable ground actions
        int num_reached_facts;        // Number of reachable boolean facts
        int num_fixpoint_iterations;  // Number of iterations to reach fixpoint
        double grounding_time_ms;     // Wall-clock time for ground()
        int naive_estimate;           // Cartesian-product upper bound
    };
    Stats get_stats() const;

private:
    const Problem& problem_;
    Stats stats_;
    int lower_bound_ = 0;
};
```

**Estimated LOC**: ~200-250 (the heavy lifting is in `BindingMatcher`)

### Step 7: Grounding Pass (Pipeline Integration)

**New files**: `passes/grounding_pass.hpp`, `passes/grounding_pass.cpp`

A `Pass` subclass that wraps `ReachabilityGrounder`:

```cpp
/// Preprocessing pass that grounds a lifted problem via reachability analysis.
///
/// If the problem is already grounded (all actions have 0 parameters), this
/// pass is a no-op — it leaves the PipelineResult unchanged. This allows the
/// same pipeline to handle both lifted and grounded inputs transparently.
///
/// When grounding is performed:
/// - result.problem is replaced with the grounded problem
/// - result.lower_bound is updated with the RPG lower bound
/// - result.proven_unsolvable is set if goals are unreachable
///
/// This pass should be the FIRST pass in the pipeline (before BooleanRPGPass
/// and SymmetryPass). After this pass runs, the problem is indistinguishable
/// from one that was grounded by the Python frontend, so all downstream
/// passes work unchanged.
class GroundingPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "reachability-grounding"; }
};
```

**Integration in `main.cpp`**:

```cpp
// === PREPROCESSING PIPELINE ===
rantanplan::GroundingPass grounding_pass;       // NEW
rantanplan::BooleanRPGPass boolean_rpg_pass;
rantanplan::NumericRPGPass numeric_rpg_pass;
rantanplan::SymmetryPass symmetry_pass;
std::vector<const rantanplan::Pass*> passes;

// 1. Grounding pass first — no-op if problem is already grounded.
passes.push_back(&grounding_pass);

// 2. RPG pass for action removal.
//    - If the grounding pass ran (lifted input), the Boolean RPG is already
//      subsumed — it would produce no additional pruning. Only the Numeric
//      RPG adds value (numeric bound checking).
//    - If the problem arrived already grounded, the Boolean RPG is still
//      needed for action removal + lower bound computation.
//    The GroundingPass sets a flag in PipelineResult indicating whether it
//    ran, so we can skip the Boolean RPG conditionally.
if (config.global.enable_action_removal) {
    // NumericRPGPass is always useful (numeric precision), even after grounding.
    // BooleanRPGPass is only useful if grounding did NOT run.
    passes.push_back(config.global.use_numeric_rpg
                     ? static_cast<const rantanplan::Pass*>(&numeric_rpg_pass)
                     : static_cast<const rantanplan::Pass*>(&boolean_rpg_pass));
    // NOTE: The BooleanRPGPass should check pipeline_result.grounding_performed
    // and skip itself if true. Alternatively, GroundingPass could be made to
    // not push BooleanRPGPass at all — but the conditional-skip approach is
    // cleaner since the pipeline is configured statically.
}
// ... rest unchanged
```

To support the conditional skip, add a field to `PipelineResult`:

```cpp
struct PipelineResult {
    Problem problem;
    bool proven_unsolvable = false;
    std::string unsolvable_reason;
    int lower_bound = 0;
    // ... existing fields ...
    bool grounding_performed = false;  // NEW: set by GroundingPass
};
```

The `BooleanRPGPass::apply()` checks this flag and returns immediately if `grounding_performed` is true.

**Estimated LOC**: ~50

### Step 8: Ground Action Construction

**New file or addition to**: `grounding/action_instantiator.hpp`, `grounding/action_instantiator.cpp`

Utility to build a ground `Action` from a lifted action + complete binding:

```cpp
/// Instantiate a lifted action with a complete parameter binding, producing
/// a fully grounded Action suitable for encoding.
///
/// What this does:
///   1. Constructs a Substitution from the binding (param name → object constant ExprID)
///   2. Applies substitute() to the precondition ExprID → ground precondition
///   3. Applies substitute() to each effect's fluent_id, value_id, and condition_id
///   4. Creates a new Action with:
///      - Name: "schema_obj1_obj2_..." (matching the convention Python uses)
///      - Parameters: empty vector (it's now ground)
///      - Ground precondition and effects
///
/// The resulting action is indistinguishable from one that Python's Grounder
/// would have produced — downstream code (encoders, parallelism, interference)
/// works unchanged.
Action instantiate_action(
    const Action& lifted_action,
    const PartialBinding& binding,
    const Problem& problem,
    ExprPool& pool);
```

**Estimated LOC**: ~80-100

### Step 9: Problem Reconstruction

Within `ReachabilityGrounder::ground()`, after the fixpoint loop, we need to build a new `Problem` with the grounded actions. This requires:

1. Create a new `Problem` sharing the same `types_` and `pool_` (via `shared_ptr`)
2. Copy objects, fluents, initial state, goals verbatim
3. Set `actions_` to the vector of instantiated ground `Action`s
4. Reassign action IDs to be contiguous `[0, N-1]`
5. Recompute `grounded_fluents_` — this should include ALL ground fluent applications that appear in any action's preconditions or effects, plus those in the initial state. The existing `collect_grounded_fluents()` scans initial state assignments; we may also want to include fluents discovered during grounding.

**Note on `collect_grounded_fluents()`**: The current implementation only collects fluents from the initial state. After grounding, we should also collect fluents that appear in action preconditions and effects. The existing grounded-from-Python path doesn't have this issue because Python's grounder produces actions where fluent applications are already ground, and the encoder discovers fluent variables from actions. A `collect_all_grounded_fluents()` method would scan both initial state and all ground action expressions.

**Estimated LOC**: ~80-100 (mostly wiring)

### Step 10: Build System and Tests

**Files**: `rantanplan/cpp/CMakeLists.txt`

1. Add new source files to CMakeLists.txt:
   ```
   src/problem/substitution.cpp
   src/grounding/fact_index.cpp
   src/grounding/binding_matcher.cpp
   src/grounding/reachability_grounder.cpp
   src/grounding/action_instantiator.cpp
   src/passes/grounding_pass.cpp
   ```

2. Build: `python3 build.py --clean`

### Step 11: Logging and Diagnostics

All new code should use `Logger::instance()` for structured output:

```
[grounding] Problem is lifted (10 action schemas, 43 objects)
[grounding] Iteration 1: 37 new facts, 12 new ground actions
[grounding] Iteration 2: 8 new facts, 5 new ground actions
[grounding] Iteration 3: 0 new facts (fixpoint reached)
[grounding] Result: 17 reachable ground actions (naive estimate: 5355, reduction: 315×)
[grounding] RPG lower bound: 4
[grounding] Grounding completed in 2.3ms
```

At `verbose` level, log each grounded action. At `debug` level, log the join matching details.

---

## File Summary

| File | Status | Estimated LOC | Description |
|---|---|---|---|
| `problem/substitution.{hpp,cpp}` | New | ~80 | ExprID tree substitution (PARAMETER → CONSTANT) |
| `grounding/fact_index.{hpp,cpp}` | New | ~150 | Fact storage with fluent-indexed lookup |
| `grounding/binding_matcher.{hpp,cpp}` | New | ~300 | Join-based parameter binding matcher |
| `grounding/reachability_grounder.{hpp,cpp}` | New | ~250 | Fixpoint loop: FactIndex + BindingMatcher → ground actions |
| `grounding/action_instantiator.{hpp,cpp}` | New | ~100 | Lifted action + binding → ground Action |
| `passes/grounding_pass.{hpp,cpp}` | New | ~50 | Pipeline pass wrapping ReachabilityGrounder |
| `problem/problem.{hpp,cpp}` | Modified | ~10 | Add `is_grounded()` method |
| `main.cpp` | Modified | ~5 | Add GroundingPass to pipeline |
| `CMakeLists.txt` | Modified | ~6 | Add new source files |
| `planner_wrapper.py` | Modified | ~15 | Add `--cpp-grounding` flag path |
| `cli.py` | Modified | ~5 | Add `--cpp-grounding` flag |

**Total**: ~970 new LOC C++ + ~25 LOC Python modifications

---

## Implementation Order

The recommended implementation order minimizes risk by building bottom-up:

1. **Python-side `--cpp-grounding` flag** — First, so we can test end-to-end as soon as C++ code compiles.

2. **`substitution.{hpp,cpp}`** — Pure function, foundational.

3. **`fact_index.{hpp,cpp}`** — Data structure, no dependencies beyond `Problem`.

4. **`binding_matcher.{hpp,cpp}`** — Depends on `FactIndex`.

5. **`action_instantiator.{hpp,cpp}`** — Depends on `substitution`.

6. **`reachability_grounder.{hpp,cpp}`** — Integrates everything.

7. **`grounding_pass.{hpp,cpp}` + `main.cpp` + `PipelineResult` changes** — Pipeline integration.

8. **End-to-end testing** — See Verification Plan below.

---

## Verification Plan

### Correctness

The primary test is a **comparison test**: run the same domains with and without `--cpp-grounding` and verify both produce valid plans.

1. **Quick regression**:
   ```bash
   python3 test.py --quick                   # default (Python grounding)
   python3 test.py --quick --cpp-grounding   # C++ grounding
   ```
   Both must pass 100/100 with the same strategies. The plans need not be identical (different grounding order → different action IDs → potentially different plans), but both must be valid.

2. **Full regression**: `python3 test.py --cpp-grounding` with all 54 domains × all strategies.

3. **Action count spot-check**: For a few representative domains (Hydropower, Rover, Zenotravel), log the number of ground actions produced by each path and verify they are the same (or that C++ produces a subset — both are correct since the Boolean RPG may prune further in the Python path).

### Performance

1. **Timing**: Compare total solve time (including grounding) for Rover pfile16, Hydropower, Satellite — expect speedup from eliminating the Python grounding subprocess + protobuf serialization of all ground actions.

2. **Memory**: The C++ grounder should use less memory than materializing all ground actions in Python + serializing to protobuf.

---

## Discussion: Leveraging the Numeric RPG During Grounding

The plan above handles numeric preconditions by **skipping them** during reachability analysis — any numeric comparison is assumed satisfiable. This is the same over-approximation as the Python-side FDI abstraction and is sound (never misses a reachable action), but may keep some actions that are numerically infeasible.

The existing Numeric RPG (`NumericRelaxedPlanningGraph`) goes further: it uses Z3 to compute numeric bounds on fluent values at each relaxation layer. This means it can determine, for example, that `fuel(plane1)` can never exceed 200 in any relaxed execution, and therefore an action requiring `(>= (fuel plane1) 500)` is unreachable.

### How It Could Be Integrated

There are two potential levels of integration:

#### Level 1: Post-Grounding Numeric Pruning (Current Architecture, No Changes Needed)

After the `GroundingPass` produces a grounded problem, the existing `NumericRPGPass` runs as a separate pipeline pass and prunes any grounded actions whose numeric preconditions are infeasible. This is exactly what happens today — the two passes are already complementary. The `GroundingPass` handles the combinatorial explosion; the `NumericRPGPass` handles numeric infeasibility.

**Advantage**: No changes to either pass. Clean separation of concerns.
**Limitation**: The grounded problem may still contain numerically-infeasible actions that consume memory and encoding time until the Numeric RPG removes them.

#### Level 2: Numeric-Aware Grounding (Tighter Integration)

During the grounding fixpoint loop, instead of blindly assuming all numeric conditions are satisfiable, maintain numeric bounds alongside the boolean `FactIndex`:

```
FactIndex        → tracks reachable boolean facts
NumericBounds    → tracks reachable intervals for numeric fluents
                   e.g., fuel(plane1) ∈ [0, 200], distance(c1,c2) ∈ [10, 10]
```

At each fixpoint iteration:
1. Find bindings using the boolean `FactIndex` + `BindingMatcher` (as before)
2. For each candidate binding, check if the numeric preconditions are satisfiable given current `NumericBounds` (using interval arithmetic or Z3)
3. If not, discard the binding (don't add it as a reachable ground action)
4. If yes, update `NumericBounds` with the action's numeric effects (increase/decrease bounds)

**Advantage**: Produces a tighter set of ground actions — fewer numerically-infeasible actions survive. This is especially valuable for domains where numeric constraints significantly limit reachability (e.g., fuel capacity prevents long trips).

**Challenge**: Numeric bound computation is expensive (Z3 queries) and must be done per-binding, not per-action-schema. For an action with 500 reachable boolean bindings, that's 500 Z3 queries per fixpoint iteration. The cost may outweigh the benefit for most domains.

**Recommended approach**: Start with Level 1 (post-grounding pruning, already implemented). Level 2 is an optimization for domains where the post-grounding numeric RPG removes a significant fraction of actions — benchmark first to see if the tighter integration is worth the complexity.

#### Level 3: Hybrid (Best of Both)

A pragmatic middle ground: run the boolean-only grounding fixpoint to completion, then do a **single pass** of numeric feasibility checking on the resulting ground actions (using interval arithmetic from the Numeric RPG) before handing off to the encoder. This is cheaper than Level 2 (one pass, not per-iteration) but tighter than Level 1 (integrated into the same preprocessing step, no separate RPG needed).

This is essentially "inline the Numeric RPG's pruning logic into the GroundingPass's output phase". Worth considering if benchmarks show the Numeric RPG removes many actions post-grounding.
