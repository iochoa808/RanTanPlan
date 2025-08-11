#pragma once

#include "../problem/problem.h"
#include "base_encoder.h"
#include "grounded_encoding_visitor.h"
#include "z3_variable_factory.h"
#include "parallelism/parallelism_strategy.h"
#include "parallelism/parallelism_factory.h"
#include <z3++.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

// This class is able to handle the encoding of grounded fluents and actions.
namespace planmt {

class ReifiedGroundedEncoder : public BaseEncoder {
public:
    // Constructor
    ReifiedGroundedEncoder(const Problem& problem, z3::context& ctx);

    // Encoding steps
    std::shared_ptr<z3::expr> encode_initial_state() override;
    std::shared_ptr<z3::expr> encode_actions(int t) override; // Encodes actions layer_idx 
    std::shared_ptr<z3::expr> encode_frames(int t) override; // Encodes frame axioms for layer_idx to layer_idx+1
    std::shared_ptr<z3::expr> encode_goal(int t) override;    // Encodes goal conditions at layer_idx
    std::shared_ptr<z3::expr> encode_parallelism(int t) override; // Encodes parallelism semantics
    
    // Strategy management
    void set_parallelism_strategy(ParallelismFactory::ParallelismType type) override;
    void set_parallelism_strategy(const std::string& strategy_name) override;
    std::string get_parallelism_strategy_name() const override;
    
    // Access to variable factory for plan extraction
    Z3VariableFactory& get_variable_factory() override { return variable_factory_; }
    const Z3VariableFactory& get_variable_factory() const override { return variable_factory_; }
    
    // Get access to parallelism strategy for plan extraction
    const ParallelismStrategy* get_parallelism_strategy() const override { return parallelism_strategy_.get(); }

private:
    // Helper function to convert expression to Z3 using visitor
    std::optional<z3::expr> convert_expression_to_z3(const Expression& expr, int timestep = -1);
    
    // Helper function to convert effect to Z3 constraint using visitor
    std::optional<z3::expr> convert_effect_to_z3(const EffectExpression& effect, int timestep);
    
    // Helper function to print EPC index for debugging
    void print_epc_index(const std::string& context) const;
    
    // Member variables
    const Problem& problem_; // The planning problem instance
    z3::context& ctx_;       // Z3 context (shared)
    Z3VariableFactory variable_factory_; // Factory for creating and managing Z3 variables

    GroundedEncodingVisitor grounded_visitor_; // Grounded encoding visitor for individual variables

    // Parallelism strategy
    std::unique_ptr<ParallelismStrategy> parallelism_strategy_;

    // Indices for the frame axioms

    // Map from grounded fluent (Expression) to vector of (Action*, EffectExpression*). For example:
    // epc_index_[at(airplane1, city1)] -> [(move_action*, effect_expr*), (fly_action*, effect_expr*)]
    // where each pair represents an action that can affect the fluent at(airplane1, city1)
    std::unordered_map<Expression, std::vector<std::pair<const Action*, const EffectExpression*>>> epc_index_;

    void build_epc_index();

    int layers_encoded_ = -1; // Tracks the highest layer for which transitions are encoded
};

} // namespace planmt
