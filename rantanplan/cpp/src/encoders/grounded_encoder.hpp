#pragma once

#include "../problem/problem.hpp"
#include "base_encoder.hpp"
#include "grounded_encoding_visitor.hpp"
#include "z3_variable_factory.hpp"
#include "parallelism/parallelism_strategy.hpp"
#include <z3++.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

// This class is able to handle the encoding of grounded fluents and actions.
namespace rantanplan {

class GroundedEncoder : public BaseEncoder {
public:
    // Constructor
    GroundedEncoder(const Problem& problem, z3::context& ctx);

    // Encoding steps
    std::shared_ptr<z3::expr> encode_initial_state() override;
    std::shared_ptr<z3::expr> encode_actions(int t) override; // Encodes actions layer_idx 
    std::shared_ptr<z3::expr> encode_frames(int t) override; // Encodes frame axioms for layer_idx to layer_idx+1
    std::shared_ptr<z3::expr> encode_goal(int t) override;    // Encodes goal conditions at layer_idx
    std::shared_ptr<z3::expr> encode_parallelism(int t) override; // Encodes parallelism semantics
    std::shared_ptr<z3::expr> encode_symmetries(int t) override;
    std::shared_ptr<z3::expr> encode_prefix_monotone(int t) override; // Front-loading symmetry breaking
    
    // Strategy management
    void set_parallelism_strategy(std::unique_ptr<ParallelismStrategy> strategy) override;
    std::string get_parallelism_strategy_name() const override;
    
    // Access to variable factory for plan extraction
    Z3VariableFactory& get_variable_factory() override { return variable_factory_; }
    const Z3VariableFactory& get_variable_factory() const override { return variable_factory_; }
    
    // Get access to parallelism strategy for plan extraction
    const ParallelismStrategy* get_parallelism_strategy() const override { return parallelism_strategy_.get(); }
    
    // Encode precondition + effect constraints for a single action at timestep t.
    // Returns nullptr if the action has no effects (nothing to encode).
    std::shared_ptr<z3::expr> encode_single_action(const Action& action, int t);

    // Create action variables for all actions at timestep t without encoding constraints.
    void ensure_action_variables(int t);

    // Plan extraction from Z3 model
    Plan extract_plan(const z3::model& model, int max_timestep) const override;
    
    // Helper functions to convert expressions/effects to Z3 using visitor (implementing base interface)
    z3::expr convert_expr_id_to_z3(ExprID id, int timestep = -1) override;
    z3::expr convert_effect_to_z3(const EffectExpression& effect, int timestep) override;
    
protected:
    
    // Helper function to print EPC index for debugging
    void print_epc_index(const std::string& context) const;
    
    // Member variables accessible to derived classes
    const Problem& problem_; // The planning problem instance
    z3::context& ctx_;       // Z3 context (shared)
    Z3VariableFactory variable_factory_; // Factory for creating and managing Z3 variables

    GroundedEncodingVisitor grounded_visitor_; // Grounded encoding visitor for individual variables

    // Parallelism strategy
    std::unique_ptr<ParallelismStrategy> parallelism_strategy_;

    // Indices for the frame axioms

    // Map from grounded fluent (ExprID) to vector of (Action*, EffectExpression*). For example:
    // epc_index_[expr_id(at(airplane1, city1))] -> [(move_action*, effect_expr*), (fly_action*, effect_expr*)]
    // where each pair represents an action that can affect the fluent at(airplane1, city1)
    std::unordered_map<ExprID, std::vector<std::pair<const Action*, const EffectExpression*>>> epc_index_;

    void build_epc_index();

public:
    const auto& get_epc_index() const { return epc_index_; }
protected:

    int layers_encoded_ = -1; // Tracks the highest layer for which transitions are encoded

private:
    // Helper methods for plan extraction (moved from SequentialPlanner)
    std::vector<const Action*> extract_parallel_actions_at_timestep(const z3::model& model, int timestep) const;
    std::vector<const Action*> topologically_sort_actions(const std::vector<const Action*>& actions) const;
};

} // namespace rantanplan
