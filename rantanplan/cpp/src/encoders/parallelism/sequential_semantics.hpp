#pragma once

#include "parallelism_strategy.hpp"

namespace rantanplan {

/**
 * @brief Sequential semantics: exactly one action can execute at each timestep
 * 
 * This strategy enforces that only one action can be executed at any given timestep,
 * implementing classical sequential planning semantics.
 */
class SequentialSemantics : public ParallelismStrategy {
public:
    SequentialSemantics() = default;
    
    std::shared_ptr<z3::expr> encode_parallelism(int timestep) override;
    
    void initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) override;
    
    std::string get_name() const override { return "SequentialSemantics"; }

    // Analyzer access methods (not applicable for sequential semantics)
    void set_interference_analyzer(std::unique_ptr<InterferenceAnalysis> analyzer) override {
        // Sequential semantics doesn't use interference analysis
        (void)analyzer;  // Suppress unused parameter warning
    }

    const InterferenceAnalysis* get_interference_analyzer() const override { return nullptr; }

    bool allows_concurrent_actions() const override { return false; }

private:
    const Problem* problem_;
    z3::context* ctx_;
    Z3VariableFactory* variable_factory_;
};

} // namespace rantanplan
