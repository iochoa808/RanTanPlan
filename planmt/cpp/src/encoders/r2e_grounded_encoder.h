#pragma once

#include "grounded_encoder.h"
#include <unordered_map>
#include <vector>

namespace planmt {

/**
 * @brief R2E (Relaxed ∃-step) SMT encoder
 * 
 * Implements the R2∃-step semantics using chain variables and global action ordering.
 * Generates constraints for preconditions, effects, carry-forward, and linking.
 */
class R2EGroundedEncoder : public GroundedEncoder {
public:
    enum class ActionOrdering {
        DEC  // Declaration order (as actions appear in input)
    };
    
    R2EGroundedEncoder(const Problem& problem, z3::context& ctx, 
                       ActionOrdering ordering = ActionOrdering::DEC);
    
    // Override encoding methods to implement R2E semantics
    std::shared_ptr<z3::expr> encode_actions(int t) override;
    std::shared_ptr<z3::expr> encode_frames(int t) override;
    std::shared_ptr<z3::expr> encode_parallelism(int t) override;

private:
    ActionOrdering action_ordering_;
    
    // Global action ordering L = [a1, a2, ..., a|A|]
    std::vector<const Action*> global_action_order_;
    
    // For each variable x, Ax = set of actions that modify x
    std::unordered_map<Expression, std::vector<const Action*>> variable_modifiers_;
    
    // Mapping ρx: {0, ..., |Ax|} → {0, ..., |A|} for each variable x
    // ρx(i) gives the index in global_action_order_ of the i-th action that modifies x
    std::unordered_map<Expression, std::vector<int>> rho_x_;
    
    // Mapping prevx: {1, ..., |A|} → {0, ..., |A|} for each variable x
    // prevx(i) gives the index of the last action before ai that modifies x
    std::unordered_map<Expression, std::vector<int>> prev_x_;
    
    // All state variables for closed-world assumption
    std::unordered_set<Expression> all_state_variables_;
    
    // Setup methods
    void build_action_ordering();
    void build_variable_modifiers();
    void build_rho_mappings();
    void build_prev_mappings();
    void collect_all_state_variables();
    
    // Constraint generation methods
    std::shared_ptr<z3::expr> encode_precondition_constraints(int t);
    std::shared_ptr<z3::expr> encode_effect_constraints(int t);
    std::shared_ptr<z3::expr> encode_linking_constraints(int t);
    
    // Helper methods
    std::unordered_map<Expression, z3::expr> create_prev_substitution(int action_index, int timestep);
    std::unordered_map<Expression, z3::expr> create_modi_substitution(int action_index, int timestep);
    z3::expr apply_substitution(const z3::expr& expr, const std::unordered_map<Expression, z3::expr>& substitution);
    std::optional<z3::expr> convert_expression_to_z3_template(const Expression& expr);
    z3::expr create_effect_value_z3(const EffectExpression& eff_expr, const z3::expr& fluent_z3,
                                   const std::unordered_map<Expression, z3::expr>& prev_substitution, int timestep);
    z3::expr encode_single_effect_with_carry_forward(const Effect& effect, 
                                                   const std::unordered_map<Expression, z3::expr>& prev_substitution,
                                                   const std::unordered_map<Expression, z3::expr>& modi_substitution, 
                                                   int timestep, int action_index, const z3::expr& action_var);
    
    // Chain variable management
    std::string get_chain_variable_name(const Expression& variable, int timestep, int action_index) const;
    z3::expr get_chain_variable(const Expression& variable, int timestep, int action_index);
    z3::expr get_prev_variable_or_chain(const Expression& variable, int timestep, int action_index);
    
    // Action index management
    int get_global_action_index(const Action* action) const;
    
    // Plan extraction for R2E semantics
    Plan extract_plan(const z3::model& model, int max_timestep) const override;
    
    // Debug method
    void debug_print_structures() const;
};

} // namespace planmt