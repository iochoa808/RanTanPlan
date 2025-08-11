#include "decision_heuristic_propagator.h"
#include <iostream>

namespace planmt {

DecisionHeuristicPropagator::DecisionHeuristicPropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), encoder_(nullptr), problem_(&problem), 
      variable_factory_(nullptr), ctx_(&solver.ctx()), solver_(&solver), goal_timestep_(-1) {
    
    // Register Z3 callbacks
    register_fixed();
    register_decide();
}

void DecisionHeuristicPropagator::initialize(z3::solver& solver, const BaseEncoder& encoder) {
    encoder_ = &encoder;
    variable_factory_ = &encoder.get_variable_factory();
    
    // Phase 2: Extract goal literals and build action mappings
    extract_goal_literals();
    build_literal_producer_mapping();
    build_action_precondition_mapping();
    
    if (Config::instance().is_info()) {
        std::cout << "DecisionHeuristic: Initialized with " << goal_literals_.size() 
                  << " goal literals, " << literal_producers_.size() << " producer mappings" << std::endl;
    }
}

void DecisionHeuristicPropagator::register_timestep_variables(int timestep) {
    goal_timestep_ = timestep;
    
    if (Config::instance().is_verbose()) {
        std::cout << "DecisionHeuristic: Registered timestep " << timestep << std::endl;
    }
}

void DecisionHeuristicPropagator::cleanup() {
    goal_literals_.clear();
    literal_producers_.clear();
    action_preconditions_.clear();
    supported_at_timestep_.clear();
    trail_.clear();
    decision_levels_.clear();
}

void DecisionHeuristicPropagator::push() {
    // TODO: Implement state saving
}

void DecisionHeuristicPropagator::pop(unsigned num_scopes) {
    // TODO: Implement state restoration
}

void DecisionHeuristicPropagator::fixed(z3::expr const& var, z3::expr const& value) {
    // TODO: Implement variable assignment tracking
    // This will be called when Z3 assigns a value to a variable
    // We can track action variable assignments here for future heuristic decisions
}

void DecisionHeuristicPropagator::decide(z3::expr const& val, unsigned bit, bool is_pos) {
    // TODO: Implement decision heuristic logic
    // This callback is called when Z3 is about to make a decision on 'val'
    // Parameters:
    //   val - the Z3 expression (variable) that Z3 wants to decide on
    //   bit - the bit index within the variable (for bit-vectors; 0 for boolean vars)
    //   is_pos - true if Z3 wants to try the positive phase first, false for negative
    
    // Example of how to use next_split() to override Z3's decision:
    /*
    // Get our preferred action variable using heuristic
    auto preferred_action = select_best_action_variable();
    
    if (preferred_action.has_value()) {
        // Call next_split to suggest our preferred variable instead of 'val'
        bool success = next_split(
            preferred_action.value(),  // e: The Z3 expression we want to branch on instead
            0,                        // idx: Bit index (0 for boolean variables, can be >0 for bit-vectors)
            Z3_L_TRUE                // phase: Preferred phase - Z3_L_TRUE (try true first), 
                                     //        Z3_L_FALSE (try false first), or Z3_L_UNDEF (no preference)
        );
        
        // next_split returns:
        //   true - Z3 accepted our suggestion and will branch on our preferred variable
        //   false - Z3 rejected our suggestion (e.g., variable already assigned) and will use original decision
        
        if (success) {
            // Z3 will now branch on preferred_action.value() with phase Z3_L_TRUE
            // This means it will first try setting the action variable to true
        } else {
            // Z3 will proceed with its original decision on 'val' with phase 'is_pos'
        }
    }
    
    // Alternative example: Override the phase of Z3's chosen variable
    // Instead of suggesting a different variable, we could just change the phase:
    // next_split(val, bit, Z3_L_FALSE);  // Try false first instead of is_pos
    */
    
    // For now, this is a no-op - let Z3 use its default decision strategy
}

z3::user_propagator_base* DecisionHeuristicPropagator::fresh(z3::context& ctx) {
    return nullptr;
}


// Extract individual literals from complex goal expressions:
// This method processes each goal in the problem and breaks down compound expressions
// (e.g., "(and (at robot loc1) (holding block))") into individual trackable literals
void DecisionHeuristicPropagator::extract_goal_literals() {
    for (size_t i = 0; i < problem_->goal_count(); ++i) {
        const Goal& goal = problem_->goal(i);
        const Expression& goal_expr = goal.goal_expression();
        
        // Recursively extract all atomic literals from the goal expression
        // This handles any goal format: CNF, DNF, simple atoms, or nested expressions
        auto literals = extract_literals_from_expression(goal_expr);
        
        // Store each extracted literal for backward chaining search
        for (const auto& literal : literals) {
            goal_literals_.insert(literal);
            
            if (Config::instance().is_debug()) {
                std::cout << "DecisionHeuristic: Found goal literal: " << literal << std::endl;
            }
        }
    }
}

// Recursive helper to extract atomic literals from any Expression structure
// Handles boolean operators (and/or/not) and converts expressions to string literals
// for tracking in the heuristic's backward chaining algorithm
std::vector<std::string> DecisionHeuristicPropagator::extract_literals_from_expression(const Expression& expr) {
    std::vector<std::string> literals;
    
    try {
        if (expr.is_atom()) {
            // Base case: atomic expression (e.g., "at_robot_loc1")
            if (expr.value().is_symbol()) {
                literals.push_back(expr.value().symbol());
            }
        } else if (expr.is_list()) {
            // Compound expression: analyze the operator and recurse on arguments
            const auto& list = expr.list();
            if (!list.empty() && list[0].is_atom() && list[0].value().is_symbol()) {
                const std::string& op = list[0].value().symbol();
                
                if (op == "and" || op == "or") {
                    // Boolean conjunction/disjunction: extract literals from all subexpressions
                    // For planning purposes, we track all literals that might need support
                    for (size_t i = 1; i < list.size(); ++i) {
                        auto sub_literals = extract_literals_from_expression(list[i]);
                        literals.insert(literals.end(), sub_literals.begin(), sub_literals.end());
                    }
                } else if (op == "not" && list.size() == 2) {
                    // Negation: prepend "not_" to the negated literals
                    auto sub_literals = extract_literals_from_expression(list[1]);
                    for (const auto& lit : sub_literals) {
                        literals.push_back("not_" + lit);
                    }
                } else {
                    // Complex predicate or function: treat as single literal
                    // e.g., "(<= (+ (value c0) 1) (value c1))" becomes a single trackable literal
                    literals.push_back(expr.to_string());
                }
            }
        }
    } catch (const std::exception& e) {
        // Safety fallback: if expression parsing fails, still create a trackable literal
        if (Config::instance().is_debug()) {
            std::cout << "DecisionHeuristic: Error extracting literal from expression: " << e.what() << std::endl;
        }
        try {
            literals.push_back(expr.to_string());
        } catch (...) {
            literals.push_back("unknown_literal");
        }
    }
    
    return literals;
}

// Analyze which actions can make each literal true
// This builds the core mapping needed for backward chaining:
// literal_producers_["at_robot_loc1"] = [move_action, teleport_action, ...]
void DecisionHeuristicPropagator::build_literal_producer_mapping() {
    for (size_t i = 0; i < problem_->action_count(); ++i) {
        const Action& action = problem_->action(i);
        
        // Examine each effect to see what literals this action can make true
        for (size_t eff_idx = 0; eff_idx < action.effect_count(); ++eff_idx) {
            const Effect& effect = action.effect(eff_idx);
            const EffectExpression& eff_expr = effect.effect_expression();
            
            // Convert effect (fluent := value) into a literal string
            // This maps planning effects to the same literal format used in goals
            std::string effect_literal;
            std::string fluent_str = eff_expr.fluent().to_string();
            std::string value_str = eff_expr.value().to_string();
            
            // Handle boolean and non-boolean effects consistently
            if (value_str == "true") {
                effect_literal = fluent_str;  // Positive boolean literal
            } else if (value_str == "false") {
                effect_literal = "not_" + fluent_str;  // Negative boolean literal  
            } else {
                effect_literal = fluent_str + "_" + value_str;  // Non-boolean assignment
            }
            
            // Add this action to the list of producers for this literal
            // Multiple actions can produce the same literal (gives heuristic choices)
            literal_producers_[effect_literal].push_back(&action);
            
            if (Config::instance().is_debug()) {
//                std::cout << "DecisionHeuristic: Action " << action.name() 
//                          << " can produce literal: " << effect_literal << std::endl;
            }
        }
    }
}

// Extract precondition literals from each action
// This creates the mapping needed for backward chaining: when an action is chosen
// to support a goal, its preconditions become new subgoals to satisfy
void DecisionHeuristicPropagator::build_action_precondition_mapping() {
    for (size_t i = 0; i < problem_->action_count(); ++i) {
        const Action& action = problem_->action(i);
        std::vector<std::string> preconditions;
        
        // Extract all precondition literals (if any) from this action
        if (action.has_precondition()) {
            const Expression& precond_expr = action.precondition();
            
            // Use same literal extraction as for goals - handles complex preconditions
            // e.g., "(and (at robot ?from) (clear ?to))" -> ["at_robot_?from", "clear_?to"]
            auto precond_literals = extract_literals_from_expression(precond_expr);
            preconditions.insert(preconditions.end(), precond_literals.begin(), precond_literals.end());
        }
        
        // Store the mapping: action -> list of precondition literals
        // This enables backward chaining: when selecting this action to support a goal,
        // add its preconditions as new subgoals in the priority queue
        action_preconditions_[&action] = std::move(preconditions);
        
        if (Config::instance().is_debug() && !action_preconditions_[&action].empty()) {
//            std::cout << "DecisionHeuristic: Action " << action.name() << " has " 
//                      << action_preconditions_[&action].size() << " precondition literals" << std::endl;
        }
    }
}

} // namespace planmt
