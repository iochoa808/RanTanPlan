#pragma once

#include "parallelism_strategy.hpp"
#include "../../analysis/interference_analysis.hpp"
#include <memory>

namespace rantanplan {

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

    std::string get_name() const override { return "ForallSemantics"; }

    // Analyzer access methods
    void set_interference_analyzer(std::unique_ptr<InterferenceAnalysis> analyzer) override {
        interference_analyzer_ = std::move(analyzer);
    }

    const InterferenceAnalysis* get_interference_analyzer() const override;

    bool allows_concurrent_actions() const override { return true; }

private:
    std::unique_ptr<InterferenceAnalysis> interference_analyzer_;
};

} // namespace rantanplan
