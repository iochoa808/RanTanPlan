#pragma once

#include "../problem/problem.h"
#include "smt_encoding_visitor.h"
#include <z3++.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <memory> // For std::shared_ptr in SymbolCache

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
    std::shared_ptr<z3::expr> encode_goal_state(int t);    // Encodes goal conditions at layer_idx
    std::shared_ptr<z3::expr> encode_parallelism(int t); // Encodes parallelism semantics

private:

    // Gets or creates a Z3 variable for a grounded fluent at a specific step
    z3::expr get_fluent_var(const Fluent& fluent, const std::vector<Object>& params, int t);
    z3::expr get_action_var(const Action& action, const std::vector<Object>& params, int t);

    // Creates a unique base string representation for vars
    std::string get_smt_var_name(const Fluent& fluent, const std::vector<Object>& params) const;
    std::string get_smt_var_name(const Fluent& fluent, const std::vector<Object>& params, int t) const;
    std::string get_smt_var_name(const Action& action, const std::vector<Object>& params) const;
    std::string get_smt_var_name(const Action& action, const std::vector<Object>& params, int t) const;
    
    // Member variables
    const Problem& problem_; // The planning problem instance
    z3::context& ctx_;       // Z3 context (shared)

    SmtEncodingVisitor::SymbolCache symbol_cache_; // Symbol table
    
    // Storage for SMT variables - outer vector is indexed by timestep. Inner map's key is the base name.
    std::vector<std::unordered_map<std::string, std::shared_ptr<z3::expr>>> state_vars_; // for fluent and extra stuff
    std::vector<std::unordered_map<std::string, std::shared_ptr<z3::expr>>> action_vars_;

    int layers_encoded_ = -1; // Tracks the highest layer for which transitions are encoded
};

} // namespace planmt
