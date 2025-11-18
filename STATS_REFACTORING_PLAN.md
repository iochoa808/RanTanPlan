# Statistics Collection Refactoring Plan

## Goals
1. **Consistent telemetry**: All components use Stats singleton for programmatic access
2. **Informative visual output**: Comprehensive timing and statistics displayed on stdout for visual monitoring (always visible)
3. **Complete data export**: All timing/memory data available via `--stats-file`
4. **No duplication**: Single source of truth for metrics
5. **ASCII-only**: No fancy characters or colors, clean and portable output

---

## Design: Two-Level Reporting

### Level 1: Informative Visual Output (stdout)
**Always printed** - Human-readable timing and statistics for visual monitoring:
```
[RPG.Boolean]    time: 147ms  |  layers: 15  |  actions: 234 (187 reachable, 47 removed)  |  mem: 12MB  |  goals: REACHABLE
[Symmetry]       time: 523ms  |  detected: 12 symmetries  |  mem: 8MB
[ARPG]           time: 891ms  |  iterations: 8  |  supporters: 342  |  mem: 15MB
[Interference]   time: 23ms   |  nodes: 234  |  edges: 1023  |  mem: 2MB
[Encoding T0]    time: 45ms   |  constraints: 1234  |  mem: 45MB
[Solving T0]     time: 123ms  |  result: UNSAT
[Encoding T1]    time: 52ms   |  constraints: 1456  |  mem: 48MB
[Solving T1]     time: 247ms  |  result: UNSAT
[Encoding T2]    time: 58ms   |  constraints: 1678  |  mem: 52MB
[Solving T2]     time: 531ms  |  result: SAT  |  plan_length: 5

=== Planning Summary ===
Total time:       2.347s
Plan found:       YES
Plan length:      5
Timesteps tried:  3
Peak memory:      52MB
========================
```

**Format**: `[Component] time: Xms | stat1: value | stat2: value | ... | mem: XMB`
- Aligned columns for easy scanning
- All relevant stats included (time, memory, key metrics)
- ASCII only (REACHABLE/UNREACHABLE, SAT/UNSAT, YES/NO)

### Level 2: Complete Stats Export (Stats singleton + file)
**All metrics** recorded for programmatic access:
```
rpg.build_time_ms=147.23
rpg.total_layers=15
rpg.total_actions=234
rpg.reachable_actions=187
symmetry.detection_time_ms=523.45
symmetry.count=12
...
```

---

## Implementation Plan

### Phase 1: Core Infrastructure (Foundation)

#### 1.1 Enhance Stats Singleton
**File**: `planmt/cpp/src/util/stats.hpp`

**Changes**:
```cpp
class Stats {
public:
    // Existing methods...

    // NEW: Thread-safe operations
    void set_threadsafe(const std::string& key, double value);
    void add_threadsafe(const std::string& key, double value = 1.0);

    // NEW: Bulk operations
    void set_multiple(const std::map<std::string, double>& values);

    // NEW: Namespace/prefix helpers
    void set_prefixed(const std::string& prefix, const std::string& key, double value);

private:
    std::mutex mutex_;  // Add mutex for thread safety
};
```

**Rationale**: Thread safety for future parallelization, convenience methods

---

#### 1.2 Create ScopedTimer RAII Class
**New File**: `planmt/cpp/src/util/scoped_timer.hpp`

**Purpose**: Automatic timing with Stats recording

```cpp
class ScopedTimer {
public:
    // Constructor: start timer
    ScopedTimer(const std::string& stats_key, bool print_on_destroy = false);

    // Destructor: record elapsed time to Stats
    ~ScopedTimer();

    // Get elapsed time without destroying
    double elapsed_ms() const;
    double elapsed_s() const;

    // Disable copy/move
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    std::string stats_key_;
    bool print_on_destroy_;
    std::chrono::high_resolution_clock::time_point start_;
};
```

**Usage Example**:
```cpp
{
    ScopedTimer timer("rpg.build_time_ms");
    // ... do work ...
}  // Automatically records timing to Stats
```

---

#### 1.3 Create StatsOutput Helper
**New File**: `planmt/cpp/src/util/stats_output.hpp`

**Purpose**: Standardized visual output format with comprehensive statistics

```cpp
class StatsOutput {
public:
    // Print component timing with all relevant metrics
    static void print_component(
        const std::string& component_name,
        double time_ms,
        double memory_mb,
        const std::vector<std::pair<std::string, std::string>>& metrics
    );

    // Print encoding timestep
    static void print_encoding(
        int timestep,
        double time_ms,
        int constraints,
        double memory_mb
    );

    // Print solving timestep
    static void print_solving(
        int timestep,
        double time_ms,
        const std::string& result,
        int plan_length = -1
    );

    // Print final summary with all aggregate stats
    static void print_summary(
        double total_time_s,
        bool found_plan,
        int plan_length,
        int timesteps_tried,
        double peak_memory_mb
    );

    // Enable/disable (respects config verbosity)
    static void set_enabled(bool enabled);

private:
    static bool enabled_;
    static constexpr int COMPONENT_WIDTH = 16;
};
```

**Example Output**:
```cpp
StatsOutput::print_component("RPG.Boolean", 147.2, 12.5, {
    {"layers", "15"},
    {"actions", "234 (187 reachable, 47 removed)"},
    {"goals", "REACHABLE"}
});
// Prints: [RPG.Boolean]    time: 147ms  |  layers: 15  |  actions: 234 (187 reachable, 47 removed)  |  mem: 12MB  |  goals: REACHABLE
```

---

### Phase 2: Migrate Components to New Infrastructure

#### 2.1 Boolean RPG
**File**: `planmt/cpp/src/analysis/relaxed_planning_graph.cpp`

**Current State**:
- Lines 70-83: Manual timing + stdout printing
- `build_time_ms_` stored internally

**Refactoring**:
```cpp
bool RelaxedPlanningGraph::build() {
    ScopedTimer timer("rpg.boolean.build_time_ms");
    auto start_mem = MemoryTracker::instance().get_current_memory_mb();

    // ... existing build logic ...

    auto reachable = count_reachable_actions();
    auto total = problem_.actions.size();
    auto removed = total - reachable;
    auto mem_used = MemoryTracker::instance().get_current_memory_mb() - start_mem;

    // Record to Stats
    Stats::instance().set("rpg.boolean.total_layers", rpg_graph_.size());
    Stats::instance().set("rpg.boolean.total_actions", total);
    Stats::instance().set("rpg.boolean.reachable_actions", reachable);
    Stats::instance().set("rpg.boolean.removed_actions", removed);
    Stats::instance().set("rpg.boolean.memory_mb", mem_used);
    Stats::instance().set("rpg.boolean.goals_reachable", goals_reachable_ ? 1.0 : 0.0);

    // Visual output with comprehensive stats
    StatsOutput::print_component("RPG.Boolean", timer.elapsed_ms(), mem_used, {
        {"layers", std::to_string(rpg_graph_.size())},
        {"actions", std::to_string(total) + " (" + std::to_string(reachable) +
                    " reachable, " + std::to_string(removed) + " removed)"},
        {"goals", goals_reachable_ ? "REACHABLE" : "UNREACHABLE"}
    });

    return goals_reachable_;
}
```

**Remove**:
- Direct `std::cout` statements (lines 70-78, 81-83)
- Internal `build_time_ms_` member (handled by Stats)

**Stats Keys**:
- `rpg.boolean.build_time_ms`
- `rpg.boolean.total_layers`
- `rpg.boolean.total_actions`
- `rpg.boolean.reachable_actions`
- `rpg.boolean.memory_mb`

---

#### 2.2 Numeric RPG
**File**: `planmt/cpp/src/analysis/numeric_relaxed_planning_graph.cpp`

**Current State**:
- Lines 1042-1082: `print_statistics()` method
- Multiple internal counters

**Refactoring**:
```cpp
bool NumericRelaxedPlanningGraph::build() {
    ScopedTimer timer("rpg.numeric.build_time_ms");
    auto start_mem = MemoryTracker::instance().get_current_memory_mb();

    // ... existing build logic ...

    auto reachable = reachable_actions_.size();
    auto total = problem_.actions.size();
    auto removed = total - reachable;
    auto mem_used = MemoryTracker::instance().get_current_memory_mb() - start_mem;

    // Record all stats
    Stats::instance().set("rpg.numeric.total_layers", layers_.size());
    Stats::instance().set("rpg.numeric.smt_queries", total_smt_queries_);
    Stats::instance().set("rpg.numeric.optimization_queries", total_optimization_queries_);
    Stats::instance().set("rpg.numeric.applicability_checks", total_applicability_checks_);
    Stats::instance().set("rpg.numeric.total_actions", total);
    Stats::instance().set("rpg.numeric.reachable_actions", reachable);
    Stats::instance().set("rpg.numeric.removed_actions", removed);
    Stats::instance().set("rpg.numeric.memory_mb", mem_used);
    Stats::instance().set("rpg.numeric.goals_reachable", goals_reachable_ ? 1.0 : 0.0);

    // Visual output with comprehensive stats
    StatsOutput::print_component("RPG.Numeric", timer.elapsed_ms(), mem_used, {
        {"layers", std::to_string(layers_.size())},
        {"actions", std::to_string(total) + " (" + std::to_string(reachable) +
                    " reachable, " + std::to_string(removed) + " removed)"},
        {"SMT queries", std::to_string(total_smt_queries_)},
        {"optimization", std::to_string(total_optimization_queries_)},
        {"goals", goals_reachable_ ? "REACHABLE" : "UNREACHABLE"}
    });

    return goals_reachable_;
}
```

**Remove**:
- `print_statistics()` method (replaced by Stats + lightweight output)
- Direct `std::cout` statements in build method
- Internal timing members

**Stats Keys**:
- `rpg.numeric.build_time_ms`
- `rpg.numeric.total_layers`
- `rpg.numeric.smt_queries`
- `rpg.numeric.optimization_queries`
- `rpg.numeric.applicability_checks`
- `rpg.numeric.total_actions`
- `rpg.numeric.reachable_actions`
- `rpg.numeric.unreachable_actions`
- `rpg.numeric.memory_mb`

---

#### 2.3 ARPG
**File**: `planmt/cpp/src/arpg/arpg.cpp`

**Current State**:
- Lines 139-143: Manual timing + stdout

**Refactoring**:
```cpp
void ARPG::build() {
    ScopedTimer timer("arpg.build_time_ms");
    auto start_mem = MemoryTracker::instance().get_current_memory_mb();

    int iterations = 0;
    // ... existing build logic ...
    iterations++;

    auto supporters = count_supporters();
    auto mem_used = MemoryTracker::instance().get_current_memory_mb() - start_mem;

    Stats::instance().set("arpg.iterations", iterations);
    Stats::instance().set("arpg.supporters_created", supporters);
    Stats::instance().set("arpg.memory_mb", mem_used);

    StatsOutput::print_component("ARPG", timer.elapsed_ms(), mem_used, {
        {"iterations", std::to_string(iterations)},
        {"supporters", std::to_string(supporters)}
    });
}
```

**Stats Keys**:
- `arpg.build_time_ms`
- `arpg.iterations`
- `arpg.supporters_created`
- `arpg.memory_mb`

---

#### 2.4 Symmetry Detection
**File**: `planmt/cpp/src/symmetries/smt_symmetry_checker.cpp`

**Current State**:
- Lines 18, 80-81: Manual timing + stdout

**Refactoring**:
```cpp
void SMTSymmetryChecker::detect_symmetries() {
    ScopedTimer timer("symmetry.detection_time_ms");
    auto start_mem = MemoryTracker::instance().get_current_memory_mb();

    // ... existing detection logic ...

    auto sym_count = detected_symmetries_.size();
    auto mem_used = MemoryTracker::instance().get_current_memory_mb() - start_mem;

    Stats::instance().set("symmetry.count", sym_count);
    Stats::instance().set("symmetry.memory_mb", mem_used);

    StatsOutput::print_component("Symmetry", timer.elapsed_ms(), mem_used, {
        {"detected", std::to_string(sym_count) + " symmetries"}
    });
}
```

**Stats Keys**:
- `symmetry.detection_time_ms`
- `symmetry.count`
- `symmetry.memory_mb`

---

#### 2.5 Interference Analysis
**File**: Multiple files in `planmt/cpp/src/encoders/parallelism/`

**Current State**:
- Direct stdout with initialization messages
- Eager variants measure time locally

**Refactoring**:
```cpp
// EagerInterferenceAnalysis constructor
EagerInterferenceAnalysis::EagerInterferenceAnalysis(...) {
    ScopedTimer timer("interference.eager.build_time_ms");
    auto start_mem = MemoryTracker::instance().get_current_memory_mb();

    // ... existing logic ...

    auto nodes = interference_graph_.node_count();
    auto edges = interference_graph_.edge_count();
    auto mem_used = MemoryTracker::instance().get_current_memory_mb() - start_mem;

    Stats::instance().set("interference.eager.nodes", nodes);
    Stats::instance().set("interference.eager.edges", edges);
    Stats::instance().set("interference.eager.memory_mb", mem_used);

    StatsOutput::print_component("Interference", timer.elapsed_ms(), mem_used, {
        {"nodes", std::to_string(nodes)},
        {"edges", std::to_string(edges)}
    });
}
```

**Apply to**:
- `EagerInterferenceAnalysis`
- `LazyInterferenceAnalysis`
- `EagerSemanticInterferenceAnalysis`
- Base `InterferenceAnalyzer`

**Stats Keys** (per variant):
- `interference.{variant}.build_time_ms`
- `interference.{variant}.nodes`
- `interference.{variant}.edges`
- `interference.{variant}.memory_mb`

---

#### 2.6 Sequential Planner
**File**: `planmt/cpp/src/planners/sequential.cpp`

**Current State**:
- Lines 173-178, 182: Manual stdout timing
- Lines 42-59: Stats recording (already good!)

**Refactoring**:
```cpp
Plan SequentialSolver::solve() {
    ScopedTimer total_timer("planner.total_time");
    double peak_memory = 0.0;

    for (int t = start_t; t <= max_steps; ++t) {
        Stats::instance().add("planner.timesteps_explored");

        // Formula creation
        int constraints_count;
        {
            ScopedTimer formula_timer("planner.formula_time_t" + std::to_string(t));
            create_formula(t);
            constraints_count = get_constraint_count();
        }

        auto current_mem = MemoryTracker::instance().get_current_memory_mb();
        peak_memory = std::max(peak_memory, current_mem);

        // Print encoding stats
        StatsOutput::print_encoding(
            t,
            Stats::instance().get("planner.formula_time_t" + std::to_string(t)),
            constraints_count,
            current_mem
        );

        // Solving
        z3::check_result result;
        {
            ScopedTimer solve_timer("planner.solve_time_t" + std::to_string(t));
            result = solver_.check();
        }

        // Print solving stats
        StatsOutput::print_solving(
            t,
            Stats::instance().get("planner.solve_time_t" + std::to_string(t)),
            result == z3::sat ? "SAT" : (result == z3::unsat ? "UNSAT" : "UNKNOWN"),
            result == z3::sat ? extract_plan().size() : -1
        );

        if (result == z3::sat) {
            auto plan_length = extract_plan().size();
            Stats::instance().set("planner.solution_timestep", t);
            Stats::instance().set("planner.plan_length", plan_length);
            Stats::instance().set("planner.peak_memory_mb", peak_memory);

            StatsOutput::print_summary(
                total_timer.elapsed_s(),
                true,
                plan_length,
                t - start_t + 1,
                peak_memory
            );

            return extract_plan();
        }
    }

    Stats::instance().set("planner.peak_memory_mb", peak_memory);
    StatsOutput::print_summary(total_timer.elapsed_s(), false, 0,
                               max_steps - start_t + 1, peak_memory);
    return Plan();
}
```

**Remove**:
- Lines 173-178: Direct stdout timing
- Line 182: Direct stdout solution message

**Keep**:
- Stats recording (lines 42-59) - already good
- Z3 statistics collection

---

#### 2.7 Achievers Analysis
**File**: `planmt/cpp/src/abstraction/achievers_analysis.cpp`

**Current State**:
- Lines 27-41: Manual timing + stdout
- Uses Stats but also prints

**Refactoring**:
```cpp
void AchieversAnalysis::analyze() {
    ScopedTimer total_timer("achievers.total_time_ms");
    auto start_mem = MemoryTracker::instance().get_current_memory_mb();

    // ARPG phase
    {
        ScopedTimer arpg_timer("achievers.arpg_time_ms");
        build_arpg();
    }

    // Analysis phase
    {
        ScopedTimer analysis_timer("achievers.analysis_time_ms");
        perform_semantic_analysis();
    }

    auto mem_used = MemoryTracker::instance().get_current_memory_mb() - start_mem;

    Stats::instance().set("achievers.conditions_analyzed", total_conditions_);
    Stats::instance().set("achievers.memory_mb", mem_used);

    StatsOutput::print_component("Achievers", total_timer.elapsed_ms(), mem_used, {
        {"ARPG", std::to_string(Stats::instance().get("achievers.arpg_time_ms")) + "ms"},
        {"analysis", std::to_string(Stats::instance().get("achievers.analysis_time_ms")) + "ms"},
        {"conditions", std::to_string(total_conditions_)}
    });
}
```

**Remove**:
- Direct stdout printing (lines 27-41)

**Stats Keys**:
- `achievers.total_time_ms`
- `achievers.arpg_time_ms`
- `achievers.analysis_time_ms`
- `achievers.conditions_analyzed`
- `achievers.memory_mb`

---

### Phase 3: Main Entry Point Integration

#### 3.1 Update main.cpp
**File**: `planmt/cpp/src/main.cpp`

**Changes**:
```cpp
int main(int argc, char* argv[]) {
    ScopedTimer main_timer("main.total_time_s");

    // Initialize StatsOutput (always enabled)
    StatsOutput::set_enabled(true);

    // ... existing preprocessing (RPG, symmetry, etc.) ...
    // All preprocessing now uses ScopedTimer + StatsOutput

    // Main solve
    Plan plan = solver.solve();

    // Final summary is printed by solver.solve()
    // No need for additional summary here

    // Stats export (existing logic)
    if (config.is_debug()) {
        Stats::instance().print_all();
    }
    if (!config.global.stats_file.empty()) {
        Stats::instance().write_to_file(config.global.stats_file);
    }

    return 0;
}
```

**Changes**:
- Remove manual RPG timing (lines 277-283) - handled by ScopedTimer
- StatsOutput always enabled (provides visual feedback)
- Final summary printed by solver, not here
- Keep existing Stats export logic

---

### Phase 4: Configuration (Optional)

**No new CLI options required** - StatsOutput is always enabled for visual feedback.

**Optional enhancement**:
If you want to allow disabling visual output:
```cpp
// Add to CLI parser
("stats-output", po::value<bool>()->default_value(true),
 "Enable statistics output during execution")
```

**Note**: Keeping it always-on is recommended for consistent user experience.

---

### Phase 5: Testing and Validation

#### 5.1 Update test.py
**File**: `test.py`

**Changes**:
- Parse lightweight output format for validation
- Check that all timing lines appear
- Verify Stats file contains expected keys

**Example validation**:
```python
def validate_lightweight_output(output: str) -> bool:
    """Ensure lightweight timing output is present."""
    required_components = ["[RPG", "[Solving]", "[Total]"]
    for component in required_components:
        if component not in output:
            return False
    return True
```

#### 5.2 Create Stats Validation Test
**New File**: `tests/test_stats_collection.cpp`

**Purpose**: Unit test Stats singleton behavior

```cpp
TEST(StatsTest, BasicSetGet) {
    Stats::instance().set("test.value", 42.0);
    EXPECT_EQ(Stats::instance().get("test.value"), 42.0);
}

TEST(StatsTest, ScopedTimerRecords) {
    {
        ScopedTimer timer("test.timer_ms");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_GT(Stats::instance().get("test.timer_ms"), 10.0);
}
```

---

## Migration Schedule: Big Bang Approach

**Strategy**: Implement all changes in one comprehensive refactoring pass.

### Implementation Order (All in one pass):

#### Step 1: Core Infrastructure
- [ ] Enhance Stats singleton with thread safety (mutex)
- [ ] Implement ScopedTimer RAII class
- [ ] Implement StatsOutput helper with all print methods
- [ ] Add unit tests for new infrastructure

#### Step 2: Component Migration (All Components)
- [ ] Boolean RPG - update build() method
- [ ] Numeric RPG - update build() method, remove print_statistics()
- [ ] ARPG - update build() method
- [ ] Symmetry Detection - update detect_symmetries()
- [ ] Interference Analysis (all 4 variants) - update constructors
- [ ] Sequential Planner - update solve() loop with new output
- [ ] Achievers Analysis - update analyze() method
- [ ] Encoders - already use Stats, no changes needed
- [ ] Propagators - already use Stats, no changes needed
- [ ] Semantics - already use Stats, no changes needed

#### Step 3: Integration & Main Entry Point
- [ ] Update main.cpp to remove manual timing
- [ ] Initialize StatsOutput
- [ ] Remove all old stdout timing code

#### Step 4: Testing & Validation
- [ ] Update test.py to validate new output format
- [ ] Run full test suite (python test.py)
- [ ] Verify all strategies work correctly
- [ ] Check stats file output is complete

#### Step 5: Documentation
- [ ] Update CLAUDE.md with new patterns
- [ ] Update README.md with new output examples
- [ ] Update NUMERIC_RPG_IMPLEMENTATION.md
- [ ] Create STATS_REFERENCE.md with all keys

**Total Estimated Time**: 3-4 days (big bang implementation)

---

## Expected Output Examples

### Before Refactoring:
```
RPG built in 147ms
Goals reachable: YES
Total layers: 15
[RPG] construction took: time=0.147s, memory=12MB, layers=15
InterferenceAnalyzer initialized with 234 actions
Building interference graph...
Interference graph built with 234 nodes and 1023 edges
T0 timing: formula=0.045s, solve=0.123s, step=0.168s, memory=45MB
T1 timing: formula=0.052s, solve=0.247s, step=0.299s, memory=48MB
T2 timing: formula=0.058s, solve=0.531s, step=0.589s, memory=52MB
*** PLAN FOUND at timestep 2 (total time: 1.056s) ***
```
*(Redundant, verbose, inconsistent, hard to scan)*

### After Refactoring:
```
[RPG.Boolean]    time: 147ms  |  layers: 15  |  actions: 234 (187 reachable, 47 removed)  |  mem: 12MB  |  goals: REACHABLE
[Interference]   time: 23ms   |  nodes: 234  |  edges: 1023  |  mem: 2MB
[Encoding T0]    time: 45ms   |  constraints: 1234  |  mem: 45MB
[Solving T0]     time: 123ms  |  result: UNSAT
[Encoding T1]    time: 52ms   |  constraints: 1456  |  mem: 48MB
[Solving T1]     time: 247ms  |  result: UNSAT
[Encoding T2]    time: 58ms   |  constraints: 1678  |  mem: 52MB
[Solving T2]     time: 531ms  |  result: SAT  |  plan_length: 5

=== Planning Summary ===
Total time:       1.056s
Plan found:       YES
Plan length:      5
Timesteps tried:  3
Peak memory:      52MB
========================
```
*(Clean, aligned, comprehensive, easy to scan, ASCII-only)*

### For Numeric Problems:
```
[RPG.Numeric]    time: 1247ms  |  layers: 23  |  actions: 187 (142 reachable, 45 removed)  |  SMT queries: 456  |  optimization: 89  |  mem: 34MB  |  goals: REACHABLE
[Interference]   time: 34ms    |  nodes: 142  |  edges: 892  |  mem: 3MB
[Encoding T0]    time: 67ms    |  constraints: 2345  |  mem: 67MB
[Solving T0]     time: 234ms   |  result: UNSAT
...
```

---

## Performance Considerations

### Overhead Analysis:
- **ScopedTimer**: Negligible (<1μs per timer)
- **Stats recording**: ~100ns per `set()` call (with mutex: ~500ns)
- **LightweightOutput**: ~10μs per print (I/O bound)

**Total overhead**: <0.1% for typical planning problems

### Memory Impact:
- Stats map: ~1KB for 50 entries
- ScopedTimer: 40 bytes per instance (stack-allocated)
- LightweightOutput: static methods, zero overhead

**Total memory impact**: <10KB

---

## Success Metrics

### Must Have:
- ✓ All components record timing to Stats
- ✓ No direct `std::cout` timing statements (except through StatsOutput)
- ✓ StatsOutput format consistent across all components
- ✓ All tests pass with new infrastructure
- ✓ Stats file contains complete data
- ✓ Memory usage tracked for all major components
- ✓ ASCII-only output (no fancy characters or color)

### Achieved:
- ✓ Thread-safe Stats for future parallelization
- ✓ Comprehensive visual feedback during execution
- ✓ Clean, scannable output format
- ✓ Single source of truth for all metrics

---

## Documentation Updates Required

1. **CLAUDE.md**: Add ScopedTimer and StatsOutput patterns
2. **README.md**: Update with new output format examples
3. **NUMERIC_RPG_IMPLEMENTATION.md**: Update with Stats keys
4. **New file**: `STATS_REFERENCE.md` - Complete list of Stats keys

---

## Conclusion

This refactoring will:
- ✓ Standardize telemetry across all components
- ✓ Provide comprehensive, visual timing and stats output for monitoring
- ✓ Enable programmatic access to all metrics via Stats singleton
- ✓ Eliminate code duplication and inconsistent reporting
- ✓ Prepare codebase for future parallelization (thread-safe Stats)
- ✓ Improve developer and user experience significantly
- ✓ Maintain simple ASCII-only output format
- ✓ Track memory usage for all major components

**Estimated effort**: 3-4 days (big bang implementation)
**Risk level**: Low (comprehensive testing, no legacy mode needed)
**Value**: High (consistency, maintainability, usability, completeness)
