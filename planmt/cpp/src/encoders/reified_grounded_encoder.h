#pragma once

#include "../problem/problem.h"
#include "grounded_encoder.h"
#include "z3_variable_factory.h"
#include <z3++.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

// This class is able to handle the encoding of grounded fluents and actions.
namespace planmt {

/**
 * @brief Specialized variable factory for reified grounded encoding
 * 
 * Extends Z3VariableFactory to handle reified clause variables used in CNF reification.
 */
class ReifiedZ3VariableFactory : public Z3VariableFactory {
private:
    // Storage for reified clause variables
    // reified_clause_vars_[timestep][clause_name] -> reified variable
    std::vector<std::unordered_map<std::string, std::shared_ptr<z3::expr>>> reified_clause_vars_;
    
public:
    explicit ReifiedZ3VariableFactory(z3::context& ctx) : Z3VariableFactory(ctx) {}
    
    // Reified clause variable management
    z3::expr get_reified_clause_variable(const std::string& clause_name, int timestep);
    std::vector<z3::expr> get_all_reified_clause_variables(int timestep) const;
    std::vector<std::pair<z3::expr, std::string>> get_all_reified_clause_variables_with_names(int timestep) const;
    
private:
    void ensure_reified_timestep_capacity(int timestep);
};


/**
 * @brief Reified grounded encoder for planning problems
 * 
 * Specializes GroundedEncoder to encode CNF preconditions/goals with reified clause variables.
 * Inherits all common functionality and only overrides action/goal encoding methods.
 */
class ReifiedGroundedEncoder : public GroundedEncoder {
private:
    ReifiedZ3VariableFactory reified_variable_factory_; // Additional factory for reified variables

    // CNF methods
    bool looks_like_a_cnf(const Expression& expr) const;
    std::vector<z3::expr> create_reified_cnf_constraints(const Expression& cnf_expr, int timestep, const std::string& prefix);
    z3::expr create_reified_clause_constraint(const Expression& clause_expr, int timestep, const std::string& clause_name);

public:
    // Constructor
    ReifiedGroundedEncoder(const Problem& problem, z3::context& ctx);

    // Override only the methods that need reification
    std::shared_ptr<z3::expr> encode_actions(int t) override;
    std::shared_ptr<z3::expr> encode_goal(int t) override;
    
    // Access to specialized reified variable factory
    ReifiedZ3VariableFactory& get_reified_variable_factory() { return reified_variable_factory_; }
    const ReifiedZ3VariableFactory& get_reified_variable_factory() const { return reified_variable_factory_; }
    
};

} // namespace planmt
