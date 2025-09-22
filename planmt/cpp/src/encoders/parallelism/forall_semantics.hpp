#pragma once

#include "parallelism_strategy.hpp"
#include "interference_analysis.hpp"
#include <memory>

namespace planmt {

/**
 * @brief Forall semantics: actions can execute in parallel if they satisfy forall constraints
 * 
 * This strategy allows multiple actions to execute simultaneously in the same timestep
 * as long as none of the actions in a step affect any other one
 * 
 * Jussi Rintanen, Keijo Heljanko, Ilkka Niemelä,
 * Planning as satisfiability: parallel plans and algorithms for plan search,
 * Artificial Intelligence, Volume 170, Issues 12–13, 2006,
 */
class ForallSemantics : public ParallelismStrategy {
public:
    ForallSemantics() = default;
    
    std::shared_ptr<z3::expr> encode_parallelism(int timestep) override;
    
    void initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) override;
    
    std::string get_name() const override { return "ForallSemantics"; }
    
    // Analyzer access methods
    const InterferenceAnalysis* get_interference_analyzer() const override;

private:
    const Problem* problem_;
    z3::context* ctx_;
    Z3VariableFactory* variable_factory_;
    std::unique_ptr<InterferenceAnalysis> interference_analyzer_;
};

} // namespace planmt
