#pragma once

#include "parallelism_strategy.h"
#include "interference_analyzer.h"
#include <memory>

namespace planmt {

/**
 * @brief Exists semantics
 *  Set of actions in a timestep can execute if there exists
 *  at least one order where actions do not interfere with actions further
 *  down the line.
 * 
 * Jussi Rintanen, Keijo Heljanko, Ilkka Niemelä,
 * Planning as satisfiability: parallel plans and algorithms for plan search,
 * Artificial Intelligence, Volume 170, Issues 12–13, 2006,
 */
class ExistsSemantics : public ParallelismStrategy {
public:
    ExistsSemantics() = default;
    
    std::shared_ptr<z3::expr> encode_parallelism(int timestep) override;
    
    void initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) override;
    
    std::string get_name() const override { return "ExistsSemantics"; }
    
    // Analyzer access methods
    const InterferenceAnalyzer* get_interference_analyzer() const override;

private:
    const Problem* problem_;
    z3::context* ctx_;
    Z3VariableFactory* variable_factory_;
    std::unique_ptr<InterferenceAnalyzer> interference_analyzer_;
};

} // namespace planmt
