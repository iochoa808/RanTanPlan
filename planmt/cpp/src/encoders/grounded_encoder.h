#pragma once

#include "../problem/problem.h"
#include "lifted_encoding_visitor.h"
#include "grounded_encoding_visitor.h"
#include <z3++.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

// This class is able to handle the encoding of grounded fluents and actions.
namespace planmt {

class GroundedEncoder {
public:
    // Constructor
    GroundedEncoder(const Problem& problem, z3::context& ctx);

    // Encoding steps
    std::shared_ptr<z3::expr> encode_initial_state();
    std::shared_ptr<z3::expr> encode_actions(int t); // Encodes actions layer_idx 
    std::shared_ptr<z3::expr> encode_frames(int t); // Encodes frame axioms for layer_idx to layer_idx+1
    std::shared_ptr<z3::expr> encode_goal(int t);    // Encodes goal conditions at layer_idx
    std::shared_ptr<z3::expr> encode_parallelism(int t); // Encodes parallelism semantics

    // Public method for getting fluent variables (used by GroundedEncodingVisitor)
    z3::expr get_fluent_var(const Fluent& fluent, const std::vector<Object>& params, int t);

private:
    // Helper function to convert expression to Z3 using visitor
    std::optional<z3::expr> convert_expression_to_z3(const Expression& expr, int timestep = -1);
    
    // Helper function to print symbol table for debugging
    void print_symbol_table(const std::string& context) const;
    
    // Helper function to print EPC index for debugging
    void print_epc_index(const std::string& context) const;

    // Gets or creates a Z3 variable for an action at a specific step
    z3::expr get_action_var(const Action& action, const std::vector<Object>& params, int t);

    // Creates a unique base string representation for vars
    std::string get_smt_var_name(const Fluent& fluent, const std::vector<Object>& params) const;
    std::string get_smt_var_name(const Fluent& fluent, const std::vector<Object>& params, int t) const;
    std::string get_smt_var_name(const Action& action, const std::vector<Object>& params) const;
    std::string get_smt_var_name(const Action& action, const std::vector<Object>& params, int t) const;
    
    // Helper method to create correctly typed variables based on fluent definitions
    z3::expr create_typed_variable(const Fluent& fluent, const std::string& var_name);
    
    // Member variables
    const Problem& problem_; // The planning problem instance
    z3::context& ctx_;       // Z3 context (shared)

    SymbolTable symbol_table_; // Unified symbol table for Z3 variables and functions
    LiftedEncodingVisitor smt_visitor_; // SMT encoding visitor (reused across methods)
    GroundedEncodingVisitor grounded_visitor_; // Grounded encoding visitor for individual variables
    
    // Storage for SMT variables - outer vector is indexed by timestep. Inner map's key is the base name.

    // state_vars_[0]["at_airplane1_city1"] -> z3::expr(bool variable for fluent at timestep 0)
    // state_vars_[1]["fuel_airplane1_10"] -> z3::expr(int variable for fluent at timestep 1)
    std::vector<std::unordered_map<std::string, std::shared_ptr<z3::expr>>> state_vars_; // for fluent and extra stuff

    // action_vars_[0]["move_airplane1_city1_city2"] -> z3::expr(bool variable for action at timestep 0)
    // action_vars_[1]["load_package1_airplane1_city1"] -> z3::expr(bool variable for action at timestep 1)
    std::vector<std::unordered_map<std::string, std::shared_ptr<z3::expr>>> action_vars_;

    // Indices for the frame axioms

    // Map from grounded fluent (Expression) to vector of (Action*, EffectExpression*). For example:
    // epc_index_[at(airplane1, city1)] -> [(move_action*, effect_expr*), (fly_action*, effect_expr*)]
    // where each pair represents an action that can affect the fluent at(airplane1, city1)
    std::unordered_map<Expression, std::vector<std::pair<const Action*, const EffectExpression*>>> epc_index_;

    void build_epc_index();

    int layers_encoded_ = -1; // Tracks the highest layer for which transitions are encoded
};

} // namespace planmt
