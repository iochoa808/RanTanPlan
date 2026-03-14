#pragma once

#include "strategy_spec.hpp"
#include "../problem/problem.hpp"
#include "../encoders/base_encoder.hpp"
#include "../planners/base_planner.hpp"
#include "../planners/propagators/propagator_strategy.hpp"
#include "../encoders/parallelism/parallelism_strategy.hpp"
#include "../encoders/parallelism/interference_analysis.hpp"
#include <z3++.h>
#include <memory>
#include <string>

namespace rantanplan {

/// Single point of responsibility for strategy compatibility validation.
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
        BaseEncoder& encoder, z3::context& ctx);
};

} // namespace rantanplan
