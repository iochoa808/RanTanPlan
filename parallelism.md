# Lifted Semantic Interference Analysis Implementation Plan

## Overview

This document outlines the implementation plan for **lifted (ungrounded) semantic interference analysis** in planMT, based on Section 4.1 of "Relaxing non-interference requirements in parallel plans" by Bofill et al.

The current implementation performs interference checking on **grounded actions** (after instantiation), which can be computationally expensive for large problems. The lifted approach checks interference between **action schemas** before grounding, dramatically reducing the number of SMT queries needed.

## Current vs. Lifted Analysis

### Current Grounded Approach
- **Input**: Grounded actions (e.g., `move(truck1, city1, city2)`, `move(truck2, city3, city4)`)
- **Complexity**: O(n²) where n = number of grounded actions (can be thousands)
- **Example**: For 100 grounded `move` actions, we need 10,000 interference checks

### Proposed Lifted Approach  
- **Input**: Action schemas (e.g., `move(?truck, ?from, ?to)`)
- **Complexity**: O(s²) where s = number of action schemas (typically < 20)
- **Example**: For 2 action schemas, we need only 4 interference checks + parameter analysis

## Implementation Architecture

### 1. Core Classes

#### `LiftedSemanticAnalyzer`
```cpp
class LiftedSemanticAnalyzer {
public:
    struct ParameterPartition {
        std::vector<std::set<std::string>> type_partitions;
        // Each partition represents parameter equality constraints
    };
    
    struct InterferencePattern {
        std::string action1_schema;
        std::string action2_schema;
        ParameterPartition partition;
        bool interferes;
    };
    
private:
    std::vector<InterferencePattern> interference_patterns_;
    std::map<std::string, std::vector<Parameter>> schema_parameters_;
};
```

#### `ParameterPartitionGenerator`
```cpp
class ParameterPartitionGenerator {
public:
    // Generate all possible parameter equality partitions for two action schemas
    std::vector<ParameterPartition> generate_partitions(
        const ActionSchema& schema1, 
        const ActionSchema& schema2
    );
    
private:
    // Calculate Bell numbers for partition generation
    std::vector<std::set<std::set<int>>> generate_set_partitions(int n);
};
```

### 2. Implementation Phases

#### Phase 1: Parameter Analysis Framework

**Files to create:**
- `planmt/cpp/src/encoders/parallelism/lifted_semantic_analyzer.h`
- `planmt/cpp/src/encoders/parallelism/lifted_semantic_analyzer.cpp`
- `planmt/cpp/src/encoders/parallelism/parameter_partition.h`
- `planmt/cpp/src/encoders/parallelism/parameter_partition.cpp`

**Key components:**
1. **Parameter Type Grouping**: Group parameters by their most general declared type
   ```cpp
   // Example: move(?truck - vehicle, ?from - city, ?to - city)
   // Results in: vehicles = {?truck}, cities = {?from, ?to}
   std::map<std::string, std::vector<std::string>> group_parameters_by_type(
       const ActionSchema& schema1, const ActionSchema& schema2);
   ```

2. **Partition Generation**: For each type group, generate all possible equality partitions
   ```cpp
   // For cities = {?from, ?to}, generate:
   // {{?from}, {?to}}     - different cities
   // {{?from, ?to}}       - same city
   std::vector<Partition> generate_type_partitions(const std::vector<std::string>& params);
   ```

3. **Cartesian Product**: Combine partitions across all types
   ```cpp
   std::vector<ParameterPartition> combine_partitions(
       const std::map<std::string, std::vector<Partition>>& type_partitions);
   ```

#### Phase 2: SMT Query Generation

**Enhanced SMT query structure:**
```cpp
class LiftedSMTQuery {
public:
    void add_parameter_constraints(const ParameterPartition& partition);
    void add_action_schemas(const ActionSchema& schema1, const ActionSchema& schema2);
    bool check_interference();
    
private:
    z3::context& ctx_;
    z3::solver solver_;
    std::map<std::string, z3::sort> parameter_sorts_;
    std::map<std::string, z3::expr> parameter_variables_;
};
```

**Example SMT encoding:**
```smt2
; For move(?truck1, ?from1, ?to1) and move(?truck2, ?from2, ?to2)
; with partition: trucks different, cities same

(declare-sort Vehicle)
(declare-sort City)
(declare-const truck1 Vehicle)
(declare-const truck2 Vehicle)
(declare-const city1 City)

; Constraint: trucks are different
(assert (not (= truck1 truck2)))

; Constraint: from1 = from2 = to1 = to2 = city1 (same city)
; Check if this leads to interference...
```

#### Phase 3: Pattern Caching and Instantiation

**Pattern storage:**
```cpp
struct InterferencePattern {
    std::string schema1_name;
    std::string schema2_name;
    ParameterPartition partition;
    bool interferes;
    std::chrono::time_point<std::chrono::steady_clock> computed_at;
};

class PatternCache {
    std::vector<InterferencePattern> patterns_;
    std::string cache_file_path_;
    
public:
    void save_to_file();
    void load_from_file();
    bool lookup_pattern(const std::string& schema1, const std::string& schema2, 
                       const ParameterPartition& partition);
};
```

**Grounded instantiation:**
```cpp
class GroundedInstantiator {
public:
    std::vector<std::pair<Action, Action>> generate_interfering_pairs(
        const InterferencePattern& pattern,
        const std::vector<Action>& grounded_actions);
        
private:
    bool matches_partition(const Action& action1, const Action& action2, 
                          const ParameterPartition& partition);
};
```

### 3. Integration Points

#### Modified SemanticInterferenceAnalyzer
```cpp
class SemanticInterferenceAnalyzer {
private:
    std::unique_ptr<LiftedSemanticAnalyzer> lifted_analyzer_;
    bool use_lifted_analysis_;
    
public:
    void build_interference_graph() override {
        if (use_lifted_analysis_ && problem_->actions().size() > LIFTED_THRESHOLD) {
            build_lifted_interference_graph();
        } else {
            build_grounded_interference_graph();
        }
    }
    
private:
    void build_lifted_interference_graph();
    void build_grounded_interference_graph(); // Current implementation
};
```

#### CLI Integration
```cpp
// Add new CLI option
parser.add_argument(
    "--lifted-analysis",
    action="store_true", 
    help="Use lifted interference analysis for better performance on large problems"
);
```

### 4. Performance Optimizations

#### Early Termination Strategies
```cpp
class OptimizedLiftedAnalyzer {
    // If schemas have no parameters of the same type, they can't interfere in most cases
    bool quick_disjoint_check(const ActionSchema& schema1, const ActionSchema& schema2);
    
    // Cache frequently occurring patterns
    std::unordered_map<std::string, bool> common_pattern_cache_;
    
    // Prioritize checking smaller partitions first
    std::vector<ParameterPartition> sort_partitions_by_complexity(
        const std::vector<ParameterPartition>& partitions);
};
```

#### Parallel Processing
```cpp
#include <thread>
#include <future>

class ParallelLiftedAnalyzer {
    std::vector<std::future<InterferencePattern>> process_partitions_parallel(
        const ActionSchema& schema1, 
        const ActionSchema& schema2,
        const std::vector<ParameterPartition>& partitions);
};
```

### 5. Example Workflow

#### Input: Two Action Schemas
```pddl
(:action move
  :parameters (?truck - vehicle ?from - city ?to - city)
  :precondition (and (at ?truck ?from) (road ?from ?to))
  :effect (and (not (at ?truck ?from)) (at ?truck ?to)))

(:action load  
  :parameters (?package - package ?truck - vehicle ?city - city)
  :precondition (and (at ?package ?city) (at ?truck ?city))
  :effect (and (not (at ?package ?city)) (in ?package ?truck)))
```

#### Step 1: Parameter Grouping
```cpp
// move: vehicles={?truck}, cities={?from, ?to}
// load: packages={?package}, vehicles={?truck}, cities={?city}
// Combined: vehicles={?truck1, ?truck2}, cities={?from, ?to, ?city}
```

#### Step 2: Partition Generation
```cpp
// Vehicles: {{?truck1}, {?truck2}} or {{?truck1, ?truck2}}
// Cities: {{?from}, {?to}, {?city}} or {{?from, ?to}, {?city}} or ... (5 partitions total)
// Cartesian product: 2 × 5 = 10 total combinations
```

#### Step 3: SMT Queries
For each of the 10 combinations, generate an SMT query like:
```smt2
; Case: different trucks, ?from = ?city, ?to different
(assert (not (= truck1 truck2)))     ; Different trucks
(assert (= from city))               ; Same pickup/delivery city  
(assert (not (= to city)))           ; Different destination
; Check: Can these actions interfere?
```

#### Step 4: Result
```cpp
InterferencePattern {
    schema1: "move",
    schema2: "load", 
    partition: vehicles_different_cities_partial_same,
    interferes: false  // They don't interfere in this configuration
}
```

### 6. Expected Performance Gains

#### Complexity Reduction
- **Current**: O(n²) where n = grounded actions (1000s)
- **Lifted**: O(s² × B^k) where s = schemas (<20), B = Bell numbers, k = parameter types
- **Real example**: 
  - Grounded: 2000² = 4M checks
  - Lifted: 10² × 2³ = 800 checks (5000× improvement)

#### Memory Usage
- **Pattern cache**: ~1KB per pattern × 100 patterns = 100KB
- **vs. Grounded cache**: ~100B per pair × 4M pairs = 400MB (4000× reduction)

### 7. Implementation Timeline

#### Week 1: Core Infrastructure
- [ ] Create `ParameterPartition` class
- [ ] Implement Bell number generation
- [ ] Create `LiftedSemanticAnalyzer` skeleton

#### Week 2: SMT Integration  
- [ ] Implement lifted SMT query generation
- [ ] Add parameter constraint encoding
- [ ] Create unit tests for small examples

#### Week 3: Integration & Optimization
- [ ] Integrate with existing `SemanticInterferenceAnalyzer`
- [ ] Add CLI options and configuration
- [ ] Implement pattern caching

#### Week 4: Testing & Validation
- [ ] Test on paper's benchmark domains
- [ ] Performance comparison vs. grounded approach
- [ ] Validate interference reduction matches paper results

### 8. Testing Strategy

#### Unit Tests
```cpp
TEST(LiftedAnalyzerTest, SingleParameterTypePartitions) {
    // Test partition generation for simple cases
}

TEST(LiftedAnalyzerTest, MultipleParameterTypes) {
    // Test Cartesian product generation
}

TEST(LiftedAnalyzerTest, SMTQueryGeneration) {
    // Verify correct SMT encoding
}
```

#### Integration Tests
```cpp
TEST(IntegratedLiftedTest, ZenotravelDomain) {
    // Compare lifted vs. grounded results on zenotravel
    EXPECT_EQ(lifted_interference_count, grounded_interference_count);
    EXPECT_LT(lifted_analysis_time, grounded_analysis_time / 100);
}
```

#### Benchmark Tests
```cpp
TEST(BenchmarkTest, ScalabilityTest) {
    // Test on problems with 10, 100, 1000, 10000 grounded actions
    // Verify lifted approach scales better
}
```

### 9. Error Handling & Robustness

#### SMT Solver Failures
```cpp
class RobustLiftedAnalyzer {
    bool try_smt_check_with_timeout(const LiftedSMTQuery& query, 
                                  std::chrono::seconds timeout = std::chrono::seconds(5));
    void fallback_to_syntactic_analysis();
    void fallback_to_grounded_analysis();
};
```

#### Memory Management
```cpp
class MemoryEfficientAnalyzer {
    // Stream large partition sets instead of loading all in memory
    std::unique_ptr<PartitionIterator> create_partition_iterator(
        const ActionSchema& schema1, const ActionSchema& schema2);
        
    // Garbage collect old patterns
    void cleanup_expired_patterns();
};
```

### 10. Future Extensions

#### Advanced Optimizations
- **Symmetry Breaking**: Detect symmetric parameters to reduce partition space
- **Dependency Analysis**: Use action dependencies to prune impossible combinations  
- **Machine Learning**: Train models to predict interference without SMT queries

#### Integration with Other Features
- **Chained Encoding**: Extend lifted analysis to support cumulative effects
- **Happening Semantics**: Add lifted analysis for function-of-compositions execution
- **Multi-level Analysis**: Combine lifted and grounded analysis adaptively

## Conclusion

This lifted semantic interference analysis will provide:

1. **Dramatic performance improvements** (100-1000× speedup on large problems)
2. **Scalability** to industrial-sized planning problems
3. **Exact semantic analysis** maintaining the precision of the grounded approach
4. **Cache-friendly** architecture for repeated problem solving

The implementation follows the paper's Section 4.1 methodology while integrating seamlessly with planMT's existing architecture.