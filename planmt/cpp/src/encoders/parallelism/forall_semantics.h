#pragma once

#include "parallelism_strategy.h"
#include "interference_analyzer.h"
#include <memory>

namespace planmt {

/**
 * @brief Forall semantics: actions can execute in parallel if they satisfy forall constraints
 * 
 * This strategy allows multiple actions to execute simultaneously as long as they
 * satisfy universal quantification constraints (e.g., no conflicting effects).
 */
class ForallSemantics : public ParallelismStrategy {
public:
    ForallSemantics() = default;
    
    std::shared_ptr<z3::expr> encode_parallelism(int timestep) override;
    
    void initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) override;
    
    std::string get_name() const override { return "ForallSemantics"; }
    
    // Analyzer access methods
    const InterferenceAnalyzer* get_interference_analyzer() const override;

private:
    const Problem* problem_;
    z3::context* ctx_;
    Z3VariableFactory* variable_factory_;
    std::unique_ptr<InterferenceAnalyzer> interference_analyzer_;
};

} // namespace planmt
