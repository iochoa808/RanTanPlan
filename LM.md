# Propositional Landmarks for RantanPlan — Implementation Plan

## Context

RantanPlan's SMT solver currently has no landmark-based reasoning. Landmarks are conditions that must hold in every valid plan — adding them as hard SMT constraints prunes the search space without cutting valid solutions. The reference procedure is Keyder, Richter, Helmert (ECAI 2010): build an AND/OR graph from the problem structure, run a fixpoint recurrence to compute landmark sets, and extract natural orderings.

The codebase is well-positioned: `AchieversAnalysis` already computes achiever mappings (condition → actions), the boolean RPG provides reachability filtering, and the pass pipeline (`Pass`/`PipelineResult`/`run_pipeline`) is designed for adding preprocessing steps.

---

## 1. New Files

### 1.1 `analysis/landmark_extractor.hpp` / `.cpp`

Core class. Builds the AND/OR graph from syntactic add-effects, runs the Keyder et al. fixpoint recurrence, produces `LandmarkData`.

```cpp
struct LandmarkData {
    std::unordered_set<int> propositional_landmarks;   // fact_ids (RPG encoding)
    std::unordered_set<int> action_landmarks;           // action indices
    std::vector<std::pair<int, int>> natural_orderings; // (pred, succ) fact_id pairs
    std::unordered_map<int, std::vector<int>> achievers;// fact_id -> action indices
    std::unordered_set<int> initial_state_facts;        // trivially satisfied at t=0
    std::unordered_map<int, ExprID> fact_id_to_expr_id; // for Z3 encoding

    bool empty() const;
    size_t landmark_count() const;
};
```

**Algorithm (run_fixpoint):**

1. Init: `lm_or_[fact_id] = {fact_id}` for all OR nodes; `lm_and_[-1] = {}` for dummy init; `lm_and_[a] = {}` for all actions.
2. Iterate until no change:
   - OR nodes: `lm_or_[f] = {f} ∪ ∩{lm_and_[a] : a achieves f}`
   - AND nodes: `lm_and_[a] = ∪{lm_or_[f] : f is precondition of a}`
3. `LM(G) = ∪{lm_or_[g] : g is a goal fact}`

**Fact ID encoding**: same as boolean RPG — `fluent_id` for positive, `-(fluent_id+1)` for negative.

### 1.2 `passes/landmark_pass.hpp` / `.cpp`

Thin pass wrapper following `BooleanRPGPass` pattern:

```cpp
class LandmarkPass : public Pass {
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "landmarks"; }
};
```

`apply()`: create `LandmarkExtractor(result.problem)`, call `extract()`, store in `result.landmark_data`.

### 1.3 `passes/symmetry_pass.hpp` / `.cpp`

Thin pass wrapper for symmetry detection:

```cpp
class SymmetryPass : public Pass {
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "symmetries"; }
};
```

`apply()`: creates its own temporary `z3::context`, runs `SMTSymmetryChecker`, stores `checker.get_all_symmetries()` → `result.symmetry_data`.

---

## 2. Modifications to Existing Files

### 2.1 `passes/pass.hpp` — Extend PipelineResult

```cpp
struct PipelineResult {
    Problem problem;
    bool proven_unsolvable = false;
    std::string unsolvable_reason;
    int lower_bound = 0;
    LandmarkData landmark_data;              // from LandmarkPass
    std::vector<SymmetryInfo> symmetry_data;  // from SymmetryPass
};
```

### 2.2 `config/config.hpp` — Config sections

```cpp
struct Landmarks {
    bool enable_landmarks = false;
} landmarks;

struct Symmetry {
    bool enable_symmetries = false;  // renamed from detect_symmetries
} symmetry;
```

### 2.3 `config/cli_parser.cpp` — Parse flags

- `--landmarks` → `config.landmarks.enable_landmarks = true`
- `--symmetries` → `config.symmetry.enable_symmetries = true`
- `--detect-symmetries` → alias for `--symmetries` (backward compat)
- Remove `--no-landmarks` (redundant, default is off)

### 2.4 `main.cpp` — Wire passes and simplify solve signature

Pipeline construction:
```cpp
if (config.landmarks.enable_landmarks) passes.push_back(&landmark_pass);
if (config.symmetry.enable_symmetries) passes.push_back(&symmetry_pass);
```

Simplified function:
```cpp
PlanGenerationResult solve_planning_problem(
    const Problem& problem,
    const PipelineResult& pipeline_result);  // instead of separate LandmarkData param
```

Inside: `encoder->set_landmark_data(pipeline_result.landmark_data)` and `encoder->set_symmetry_data(pipeline_result.symmetry_data)`.

### 2.5 `encoders/base_encoder.hpp` — Generic data setters

```cpp
virtual void set_landmark_data(const LandmarkData& data) { landmark_data_ = data; }
virtual void set_symmetry_data(std::vector<SymmetryInfo> data) { symmetry_data_ = std::move(data); }
virtual std::shared_ptr<z3::expr> encode_landmarks(int horizon) = 0;
virtual std::shared_ptr<z3::expr> encode_symmetries(int t) = 0;

protected:
    LandmarkData landmark_data_;
    std::vector<SymmetryInfo> symmetry_data_;
```

### 2.6 `encoders/grounded_encoder.hpp` / `.cpp`

**Remove** from header:
- `std::vector<ObjectSwap> detected_symmetries_`
- `std::unique_ptr<SMTSymmetryChecker> symmetry_checker_`
- `void analyze_symmetries()`

**Remove** from constructor: `analyze_symmetries()` call.

**`encode_symmetries()`**: Use `symmetry_data_` directly. Each `SymmetryInfo` has `variable_pairs` and `action_pairs` — no need for checker lookups. Guard on `symmetry_data_.empty()` instead of `config.symmetry.detect_symmetries`.

**`encode_landmarks()`**: Only ordering constraints (no disjunctive constraints — they're unsound for incremental solving). Incremental encoding tracked by `landmarks_encoded_up_to_`.

### 2.7 Planners — Remove config guards

**`sequential.cpp`**: Remove `if (config.symmetry.detect_symmetries)` around `encode_symmetries()`. Always call it — returns `true` when `symmetry_data_` is empty.

**`double_tail_planner.cpp`**: Same treatment.

### 2.8 Python side

- `cli.py`: `--landmarks`, `--symmetries` (rename from `--detect-symmetries`)
- `planner_wrapper.py`: forward flags to C++ binary
- `benchmark.py`: support `--landmarks` and `--symmetries` flags

### 2.9 `CMakeLists.txt`

Add:
```
src/analysis/landmark_extractor.cpp
src/passes/landmark_pass.cpp
src/passes/symmetry_pass.cpp
```

---

## 3. Data Flow Summary

```
[Problem from protobuf]
        |
        v
[RPG Pass] — removes unreachable actions, computes lower bound
        |
        v
[LandmarkPass] — AND/OR graph → LM(G), orderings, achievers
        |
        v
[SymmetryPass] — SMT-based object swap detection → SymmetryInfo
        |
        v
[solve_planning_problem(problem, pipeline_result)]
        |
        v
encoder->set_landmark_data(pipeline_result.landmark_data)
encoder->set_symmetry_data(pipeline_result.symmetry_data)
        |
        v
[Planner search loop]
  for each horizon H:
    solver_.add(encoder_.encode_landmarks(H))
    solver_.add(encoder_.encode_symmetries(t))
    solver_.add(implies(goal_lit, encode_goal(H)))
    solver_.check(assumptions)
```

---

## 4. Implementation Order

| Step | What | Files |
|------|------|-------|
| 1 | `LandmarkData` struct + `LandmarkExtractor` | `analysis/landmark_extractor.{hpp,cpp}` |
| 2 | `LandmarkPass` + `SymmetryPass` wrappers | `passes/landmark_pass.{hpp,cpp}`, `passes/symmetry_pass.{hpp,cpp}` |
| 3 | Extend `PipelineResult` with both data types | `passes/pass.hpp` |
| 4 | Config + CLI parsing (`--landmarks`, `--symmetries`) | `config/config.hpp`, `config/cli_parser.cpp` |
| 5 | Wire passes + simplify `solve_planning_problem` | `main.cpp` |
| 6 | Encoder: `set_symmetry_data()`, remove `analyze_symmetries()` | `base_encoder.hpp`, `grounded_encoder.{hpp,cpp}` |
| 7 | `encode_landmarks()` + `encode_symmetries()` using pipeline data | `grounded_encoder.cpp` |
| 8 | Remove config guards in planners | `sequential.cpp`, `double_tail_planner.cpp` |
| 9 | Python CLI/wrapper updates | `cli.py`, `planner_wrapper.py`, `benchmark.py` |
| 10 | CMakeLists.txt | `CMakeLists.txt` |
| 11 | Build + test | `python3 build.py && python3 test.py --quick` |

---

## 5. Testing & Verification

1. **Build**: `python3 build.py`
2. **Regression**: `python3 test.py --quick` without flags — no behavior change
3. **Landmarks**: Test with `--landmarks` on depots, zenotravel, rover
4. **Symmetries**: Test with `--symmetries` on depots (has symmetric objects)
5. **Both**: `--landmarks --symmetries` combined
6. **Backward compat**: `--detect-symmetries` still works
7. **Correctness**: landmarks only constrain orderings, never cut valid plans
