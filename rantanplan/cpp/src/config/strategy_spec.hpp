#pragma once

#include <string>

namespace rantanplan {

enum class EncoderFamily { Grounded, Chained, R2E };
enum class SemanticsKind { Sequential, Forall, Exists };
enum class InterferenceKind { EagerSyntactic, EagerSemantic, LazySyntactic, LazySemantic };
enum class PropagatorKind { Null, Forall, LazyForall, Exists, DecisionHeuristic };
enum class PlannerKind { Sequential, DoubleTail };

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

} // namespace rantanplan
