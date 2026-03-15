#pragma once

#include <string>

namespace rantanplan {

enum class EncoderFamily { Grounded, Chained, R2E };
enum class SemanticsKind { Sequential, Forall, Exists };
enum class InterferenceKind { EagerSyntactic, EagerSemantic, LazySyntactic, LazySemantic };
enum class PropagatorKind { Null, Forall, LazyForall, Exists, DecisionHeuristic };
enum class PlannerKind { Sequential, DoubleTail, BranchAndBound };

struct StrategySpec {
    EncoderFamily encoder;
    SemanticsKind semantics;
    InterferenceKind interference;
    PropagatorKind propagator;
    PlannerKind planner;
};

// Derived queries — pure functions of the spec, no virtuals needed.

inline bool supports_formula_export(const StrategySpec& spec) {
    return spec.propagator == PropagatorKind::Null &&
           spec.planner == PlannerKind::Sequential;
}

inline bool uses_double_tail(const StrategySpec& spec) {
    return spec.planner == PlannerKind::DoubleTail;
}

inline bool uses_branch_and_bound(const StrategySpec& spec) {
    return spec.planner == PlannerKind::BranchAndBound;
}

/// Returns true if SDAC cost evaluation is unsound with this strategy.
/// Exists semantics allows multiple actions per timestep as long as some
/// serialization exists, but costs are evaluated at x_t — the serialized
/// cost may differ. Sequential and forall evaluate costs correctly.
/// R2E is safe: its fixed declaration order gives each action a deterministic
/// intermediate state via chain variables, and convert_cost_to_z3() evaluates
/// costs at that state (σ^t_{prev(i)}), not x_t.
inline bool sdac_unsafe(const StrategySpec& spec) {
    return spec.semantics == SemanticsKind::Exists;
}

} // namespace rantanplan
