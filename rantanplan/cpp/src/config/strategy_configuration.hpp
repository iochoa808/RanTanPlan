#pragma once

#include "../problem/problem.hpp"
#include "../encoders/base_encoder.hpp"
#include "../planners/propagators/propagator_strategy.hpp"
#include "../encoders/parallelism/parallelism_strategy.hpp"
#include "../encoders/parallelism/interference_analysis.hpp"
#include <z3++.h>
#include <memory>
#include <string>

namespace rantanplan {

/**
 * @brief Base interface for strategy configurations
 *
 * Each strategy encapsulates a complete, validated configuration of components
 * that are guaranteed to work together correctly.
 */
class StrategyConfiguration {
public:
    virtual ~StrategyConfiguration() = default;

    // Component creation methods
    virtual std::unique_ptr<BaseEncoder> create_encoder(
        const Problem& problem, z3::context& ctx) const = 0;

    virtual std::unique_ptr<ParallelismStrategy> create_parallelism() const = 0;

    virtual std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& solver, const Problem& problem, const BaseEncoder& encoder) const = 0;

    virtual std::unique_ptr<InterferenceAnalysis> create_interference(
        const Problem& problem) const = 0;

    // Capability queries (replaces runtime type checks)
    virtual bool needs_parallelism_encoding() const = 0;
    virtual bool supports_formula_export() const { return false; }

    // Strategy name
    virtual std::string get_name() const = 0;
};

} // namespace rantanplan
