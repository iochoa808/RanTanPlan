#pragma once

#include "strategy_spec.hpp"
#include "../problem/problem.hpp"
#include "../encoders/base_encoder.hpp"
#include "../planners/base_planner.hpp"
#include "../planners/propagators/propagator_strategy.hpp"
#include "../encoders/parallelism/parallelism_strategy.hpp"
#include "../encoders/parallelism/interference_analysis.hpp"
#include "../passes/pass.hpp"
#include <z3++.h>
#include <memory>
#include <string>
#include <vector>

namespace rantanplan {

/// Single point of responsibility for strategy compatibility validation
/// and component creation.
///
/// All rules about which component combinations are legal live here —
/// both pure spec constraints (e.g. lazy interference + null propagator)
/// and spec-vs-config constraints (e.g. double-tail + non-linear schedule).
/// No other code should validate strategy compatibility.
class StrategyFactory {
public:
    /// Validate that a spec is internally consistent and compatible with
    /// the given configuration options.
    /// Throws std::invalid_argument with a clear message on failure.
    static void validate(const StrategySpec& spec,
                         const std::string& strategy_name,
                         const std::string& horizon_schedule);

    /// Adjust a strategy spec for problem-specific constraints (e.g. SDAC +
    /// exists-step downgrade). Mutates spec in place and logs adjustments.
    static void adjust_spec(StrategySpec& spec, const Problem& problem);

    /// Configure a planner after creation with pipeline results (e.g. SDAC
    /// cost lower bounds for B&B). Handles dynamic dispatch internally.
    static void configure_planner(BasePlanner& planner, const StrategySpec& spec,
                                  const PipelineResult& pipeline_result);

    static std::unique_ptr<BaseEncoder> create_encoder(
        const StrategySpec& spec, const Problem& problem, z3::context& ctx);

    static std::unique_ptr<ParallelismStrategy> create_parallelism(
        const StrategySpec& spec);

    static std::unique_ptr<InterferenceAnalysis> create_interference(
        const StrategySpec& spec, const Problem& problem);

    static std::unique_ptr<PropagatorStrategy> create_propagator(
        const StrategySpec& spec, z3::solver& solver,
        const Problem& problem, const BaseEncoder& encoder);

    static std::unique_ptr<BasePlanner> create_planner(
        const StrategySpec& spec, const Problem& problem,
        BaseEncoder& encoder, z3::context& ctx,
        const InterferenceAnalysis* interference = nullptr);

    /// Whether the given strategy + problem combination requires the
    /// NumericRPG pass in the pipeline (e.g. B&B + SDAC).
    static bool needs_numeric_rpg(const StrategySpec& spec, const Problem& problem);
};

} // namespace rantanplan
