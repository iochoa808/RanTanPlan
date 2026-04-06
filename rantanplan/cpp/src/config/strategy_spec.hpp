#pragma once

#include <string>
#include <stdexcept>

namespace rantanplan {

enum class EncoderFamily { Grounded, Chained, R2E };
enum class SemanticsKind { Sequential, Forall, Exists };
enum class InterferenceKind { None, EagerSyntactic, EagerSemantic, LazySyntactic, LazySemantic };
enum class PropagatorKind { Null, Forall, LazyForall, Exists, FrameExists, StateAwareExists };
enum class PlannerKind { Sequential, DoubleTail, BranchAndBound, PDLA, PDLASelective };

/// Search mode — orthogonal to strategy (encoding/semantics/interference/propagator).
///   Satisficing: find first valid plan, return immediately.
///   Optimal:     find cost-optimal plan with proof (BranchAndBound).
///   Anytime:     keep finding improving plans via B&B, return best at end.
enum class SearchMode { Satisficing, Optimal, Anytime };

struct StrategySpec {
    EncoderFamily encoder;
    SemanticsKind semantics;
    InterferenceKind interference;
    PropagatorKind propagator;
    PlannerKind planner;
};

// ---------------------------------------------------------------------------
// SearchMode helpers
// ---------------------------------------------------------------------------

inline SearchMode parse_search_mode(const std::string& mode) {
    if (mode.empty() || mode == "satisficing") return SearchMode::Satisficing;
    if (mode == "optimal")  return SearchMode::Optimal;
    if (mode == "anytime")  return SearchMode::Anytime;
    throw std::invalid_argument(
        "Unknown mode: '" + mode + "'. Valid values: satisficing, optimal, anytime");
}

/// Resolve the final PlannerKind from the SearchMode and the StrategySpec.
///   satisficing → use the strategy's default planner (Sequential or DoubleTail).
///   optimal/anytime → always BranchAndBound; error if strategy is DoubleTail.
inline PlannerKind resolve_planner_kind(SearchMode mode, const StrategySpec& spec) {
    if (mode == SearchMode::Satisficing) return spec.planner;
    if (spec.planner == PlannerKind::DoubleTail) {
        std::string mode_str = (mode == SearchMode::Optimal) ? "optimal" : "anytime";
        throw std::invalid_argument(
            "Mode '" + mode_str + "' is incompatible with double-tail strategies. "
            "Use a non-dt strategy (e.g., 'fl' instead of 'fl-dt').");
    }
    if (spec.planner == PlannerKind::PDLA ||
        spec.planner == PlannerKind::PDLASelective) {
        std::string mode_str = (mode == SearchMode::Optimal) ? "optimal" : "anytime";
        throw std::invalid_argument(
            "Mode '" + mode_str + "' is incompatible with PDLA strategies.");
    }
    return PlannerKind::BranchAndBound;
}

// ---------------------------------------------------------------------------
// Derived queries — pure functions of the spec, no virtuals needed.
// ---------------------------------------------------------------------------

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

/// Overload that accounts for the search mode override.
inline bool uses_branch_and_bound(SearchMode mode, const StrategySpec& spec) {
    return resolve_planner_kind(mode, spec) == PlannerKind::BranchAndBound;
}

/// Returns true if SDAC cost evaluation is unsound with this strategy.
inline bool sdac_unsafe(const StrategySpec& spec) {
    return spec.semantics == SemanticsKind::Exists;
}

/// Returns true if the strategy uses a PDLA planner variant.
inline bool uses_pdla(const StrategySpec& spec) {
    return spec.planner == PlannerKind::PDLA ||
           spec.planner == PlannerKind::PDLASelective;
}

} // namespace rantanplan
