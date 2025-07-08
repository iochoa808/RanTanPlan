#pragma once

#include "parallelism_strategy.h"
#include "interference_analyzer.h"
#include <memory>

namespace planmt {

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

private:
    const Problem* problem_;
    z3::context* ctx_;
    Z3VariableFactory* variable_factory_;
};

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

private:
    const Problem* problem_;
    z3::context* ctx_;
    Z3VariableFactory* variable_factory_;
    std::unique_ptr<InterferenceAnalyzer> interference_analyzer_;
};

/**
 * @brief Exists semantics: at least one action must execute at each timestep
 * 
 * This strategy enforces that at least one action must be executed at any given timestep,
 * allowing for unlimited parallelism as long as some action is taken.
 */
class ExistsSemantics : public ParallelismStrategy {
public:
    ExistsSemantics() = default;
    
    std::shared_ptr<z3::expr> encode_parallelism(int timestep) override;
    
    void initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) override;
    
    std::string get_name() const override { return "ExistsSemantics"; }

private:
    const Problem* problem_;
    z3::context* ctx_;
    Z3VariableFactory* variable_factory_;
    std::unique_ptr<InterferenceAnalyzer> interference_analyzer_;
};

} // namespace planmt
