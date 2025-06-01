#include "grounded_encoder.h"
#include <iostream>
#include "problem/visitors/print_visitor.h"
#include "problem/visitors/expression_visitor.h"

namespace planmt {

// Constructor
GroundedEncoder::GroundedEncoder(const Problem& problem, z3::context& ctx)
    : problem_(problem), ctx_(ctx), symbol_table_(), smt_visitor_(ctx_, symbol_table_, &problem_), grounded_visitor_(ctx_, this, &problem_) {
    // Initialize storage for state and action variables
    state_vars_.clear();
    action_vars_.clear();
    layers_encoded_ = -1;
}

// Helper function to convert expression to Z3 using visitor
std::optional<z3::expr> GroundedEncoder::convert_expression_to_z3(const Expression& expr, int timestep) {
    grounded_visitor_.clear();
    
    // Set timestep for temporal encoding if provided
    if (timestep >= 0) {
        grounded_visitor_.set_timestep(timestep);
    } else {
        grounded_visitor_.clear_timestep();
    }
    
    accept_visitor(expr, grounded_visitor_);
    
    // Clear timestep after use
    grounded_visitor_.clear_timestep();
    
    return grounded_visitor_.get_result();
}

// Encoding steps
std::shared_ptr<z3::expr> GroundedEncoder::encode_initial_state() {
    z3::expr initial_state_formula = ctx_.bool_val(true);
    
    // Process each assignment in the initial state at timestep 0
    for (const auto& assignment : problem_.initial_state()) {
        auto fluent_expr = convert_expression_to_z3(assignment.fluent(), 0);
        auto value_expr = convert_expression_to_z3(assignment.value(), 0);
        
        if (!fluent_expr || !value_expr) {
            std::cerr << "Error: Failed to encode assignment in initial state" << std::endl;
            continue;
        }
        
        // Create and add equality constraint: fluent = value
        initial_state_formula = initial_state_formula && (*fluent_expr == *value_expr);
    }
    std::cout << "Initial state SMT formula: " << initial_state_formula << std::endl;
    
    // Print symbol table after encoding initial state
    print_symbol_table("After encoding initial state");
    
    return std::make_shared<z3::expr>(initial_state_formula);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_actions(int t) {
    // TODO: Implement action encoding for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_frames(int t) {
    // TODO: Implement frame axioms encoding for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_goal(int t) {
    // Retrieve goals from the problem
    const auto& goals = problem_.goals();
    
    if (goals.empty()) {
        // If no goals, return true (vacuously satisfied)
        auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
        return expr;
    }
    
    // Convert each goal expression to Z3 formula and collect them
    std::vector<z3::expr> goal_formulas;
    goal_formulas.reserve(goals.size());
    
    for (const auto& goal : goals) {
        try {
            auto z3_goal = convert_expression_to_z3(goal.goal_expression(), t);
            if (z3_goal) {
                goal_formulas.push_back(*z3_goal);
                std::cout << "Goal encoded: " << *z3_goal << std::endl;
            } else {
                std::cerr << "Warning: Failed to encode goal expression: " << goal.to_string() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error encoding goal: " << e.what() << std::endl;
        }
    }
    
    if (goal_formulas.empty()) {
        std::cerr << "Warning: No goals were successfully encoded, returning false" << std::endl;
        auto expr = std::make_shared<z3::expr>(ctx_.bool_val(false));
        return expr;
    }
    
    // Combine all goal formulas with logical AND
    z3::expr goal_conjunction = goal_formulas[0];
    for (size_t i = 1; i < goal_formulas.size(); ++i) {
        goal_conjunction = goal_conjunction && goal_formulas[i];
    }
    
    std::cout << "Goal SMT formula at timestep " << t << ": " << goal_conjunction << std::endl;
    
    // Print symbol table after encoding goal
    print_symbol_table("After encoding goal at timestep " + std::to_string(t));
    
    return std::make_shared<z3::expr>(goal_conjunction);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_parallelism(int t) {
    // TODO: Implement parallelism constraints for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

// Private helper methods
z3::expr GroundedEncoder::get_fluent_var(const Fluent& fluent, const std::vector<Object>& params, int t) {
    std::string var_name = get_smt_var_name(fluent, params, t);
    
    // Ensure we have enough timesteps allocated
    while (static_cast<int>(state_vars_.size()) <= t) {
        state_vars_.emplace_back();
    }
    
    auto& timestep_vars = state_vars_[t];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        // Create new variable with correct type based on fluent's value type
        z3::expr new_var = create_typed_variable(fluent, var_name);
        timestep_vars[var_name] = std::make_shared<z3::expr>(new_var);
        return new_var;
    }
    return *(it->second);
}

z3::expr GroundedEncoder::get_action_var(const Action& action, const std::vector<Object>& params, int t) {
    std::string var_name = get_smt_var_name(action, params, t);
    
    // Ensure we have enough timesteps allocated
    while (static_cast<int>(action_vars_.size()) <= t) {
        action_vars_.emplace_back();
    }
    
    auto& timestep_vars = action_vars_[t];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        // Create new variable using the visitor to ensure consistency
        z3::expr new_var = smt_visitor_.create_bool_variable(var_name);
        timestep_vars[var_name] = std::make_shared<z3::expr>(new_var);
        return new_var;
    }
    return *(it->second);
}

std::string GroundedEncoder::get_smt_var_name(const Fluent& fluent, const std::vector<Object>& params) const {
    std::string name = fluent.name();
    for (const auto& param : params) {
        name += "_" + param.name();
    }
    return name;
}

std::string GroundedEncoder::get_smt_var_name(const Fluent& fluent, const std::vector<Object>& params, int t) const {
    return get_smt_var_name(fluent, params) + "_" + std::to_string(t);
}

std::string GroundedEncoder::get_smt_var_name(const Action& action, const std::vector<Object>& params) const {
    std::string name = action.name();
    for (const auto& param : params) {
        name += "_" + param.name();
    }
    return name;
}

std::string GroundedEncoder::get_smt_var_name(const Action& action, const std::vector<Object>& params, int t) const {
    return get_smt_var_name(action, params) + "_" + std::to_string(t);
}

z3::expr GroundedEncoder::create_typed_variable(const Fluent& fluent, const std::string& var_name) {
    Expression::Type fluent_type = fluent.get_type_enum();
    
    switch (fluent_type) {
        case Expression::Type::BOOLEAN:
            // Boolean fluents (predicates)
            return smt_visitor_.create_bool_variable(var_name);
        case Expression::Type::INTEGER:
            // Integer fluents
            return smt_visitor_.create_int_variable(var_name);
        case Expression::Type::REAL:
            // Real fluents
            return smt_visitor_.create_real_variable(var_name);
        case Expression::Type::OBJECT:
            // Object fluents are typically mapped to integers in SMT encoding
            return smt_visitor_.create_int_variable(var_name);
        case Expression::Type::UNKNOWN:
        default:
            // For unknown types, default to integer
            return smt_visitor_.create_int_variable(var_name);
    }
}

void GroundedEncoder::print_symbol_table(const std::string& context) const {
    std::cout << "\n===== Symbol Table: " << context << " =====" << std::endl;
    std::cout << "Total symbols: " << symbol_table_.size() << std::endl;
    
    if (symbol_table_.empty()) {
        std::cout << "(empty)" << std::endl;
    } else {
        for (const auto& [name, z3_object] : symbol_table_) {
            std::cout << "  " << name << " -> ";
            if (std::holds_alternative<z3::expr>(z3_object)) {
                const auto& expr = std::get<z3::expr>(z3_object);
                std::cout << "expr: " << expr << " (sort: " << expr.get_sort() << ")";
            } else if (std::holds_alternative<z3::func_decl>(z3_object)) {
                const auto& func_decl = std::get<z3::func_decl>(z3_object);
                std::cout << "func_decl: " << func_decl << " (arity: " << func_decl.arity() << ")";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "===== End Symbol Table =====" << std::endl << std::endl;
}

} // namespace planmt