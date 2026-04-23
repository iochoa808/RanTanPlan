#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "../analysis/arithmetic_profile.hpp"
#include "../config/strategy_spec.hpp"
#include "../grounding/interval.hpp"
#include "../problem/problem.hpp"
#include "../symmetries/smt_symmetry_checker.hpp"
#include "local_invariants.hpp"

namespace rantanplan {

// Forward declarations
class InterferenceAnalysis;
class AchieversAnalysis;
class NumericRelaxedPlanningGraph;

/**
 * @brief Accumulated state threaded through a preprocessing pipeline.
 *
 * Each pass reads and updates this struct. The pipeline short-circuits
 * if any pass sets proven_unsolvable to true.
 */
struct PipelineResult {
    Problem problem;                    ///< Current problem (transformed by each pass)
    bool proven_unsolvable = false;     ///< True if any pass proved unsolvability
    std::string unsolvable_reason;      ///< Name of the pass that proved unsolvability
    int lower_bound = 0;               ///< Max lower bound across all passes so far
    std::optional<Problem> original_problem; ///< Pre-grounding snapshot (for symmetry detection)
    std::vector<SymmetryInfo> symmetry_data; ///< From SymmetryAnalysisPass
    std::vector<double> sdac_cost_lower_bounds; ///< SDAC cost lower bounds (from ActionPruningPass); empty if not SDAC
    ArithmeticProfile arithmetic_profile = ArithmeticProfile::LINEAR; ///< Numeric constraint class (set after pipeline)
    StrategySpec resolved_spec{};               ///< Finalized strategy spec (from StrategyResolutionPass)
    std::unique_ptr<InterferenceAnalysis> interference; ///< Pre-built interference analyzer (from InterferencePass)
    std::unique_ptr<AchieversAnalysis> achievers; ///< Pre-built achiever analysis (from ActionPruningPass)
    std::unique_ptr<NumericRelaxedPlanningGraph> fixpoint_rpg; ///< Fixpoint RPG for reuse across passes
    std::unordered_map<ExprID, Interval> tightened_bounds; ///< Tightened numeric bounds (from ActionPruningPass)
    std::vector<ConservationConstraint> conservation_laws; ///< Conservation laws (from ActionPruningPass)
    StateConstraints state_constraints; ///< State constraints from InvariantOraclePass
};

/**
 * @brief Abstract base class for preprocessing passes.
 *
 * A pass reads and updates a PipelineResult in place. It may transform
 * the problem, set a lower bound, or prove unsolvability.
 *
 * Passes are stateless — all state flows through PipelineResult.
 */
class Pass {
public:
    /// Read and update the pipeline result. Called exactly once per pipeline run.
    virtual void apply(PipelineResult& result) const = 0;

    /// Human-readable name for logging and diagnostics.
    virtual std::string name() const = 0;

    virtual ~Pass() = default;
};

} // namespace rantanplan
