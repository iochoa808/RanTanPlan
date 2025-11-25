#include "decision_heuristic_propagator.hpp"
#include "../../abstraction/achievers_analysis.hpp"
#include "../../config/config.hpp"
#include "../../util/memory_tracker.hpp"
#include "../../util/stats.hpp"
#include "../../encoders/z3_variable_factory.hpp"
#include "../../encoders/parallelism/interference_analysis.hpp"
#include <iostream>
#include <set>
#include <algorithm>
#include <functional>
#include <queue>
#include <cassert>

namespace rantanplan {

DecisionHeuristicPropagator::DecisionHeuristicPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder)
    : z3::user_propagator_base(&solver), solver_(&solver), problem_(&problem), encoder_(&encoder),
     variable_factory_(&encoder.get_variable_factory()),
     parallelism_strategy_(encoder.get_parallelism_strategy()),
     interference_analyzer_(parallelism_strategy_->get_interference_analyzer()), 
     achievers_analysis_(problem),
     cycle_count_(0),
     current_goal_step_(1),
     reification_counter_(0) {

    // Define callbacks for the user propagator
    register_fixed();
    register_final();
    register_decide();

    // pre-allocate vector for up to max_steps timesteps
    // and pre-reserve capacity in each timestep map based on number of conditions
    reification_vars_per_timestep_.resize(Config::instance().planner.max_steps + 1);
    condition_values_per_timestep_.resize(Config::instance().planner.max_steps + 1);

    const auto& all_conditions = achievers_analysis_.get_all_conditions();
    for (auto& timestep_map : reification_vars_per_timestep_) {
        timestep_map.reserve(all_conditions.size());
    }
    for (auto& timestep_map : condition_values_per_timestep_) {
        timestep_map.reserve(all_conditions.size());
    }

    // Initialize active/inactive action tracking maps for all timesteps
    for (int timestep = 0; timestep < Config::instance().planner.max_steps + 1; ++timestep) {
        active_actions_per_timestep_[timestep] = std::unordered_set<int>();
        inactive_actions_per_timestep_[timestep] = std::unordered_set<int>();
    }

    // Set Z3 option to persist clauses for user propagator based on config
    solver.set("smt.up.persist_clauses", Config::instance().global.persist_clauses);
}

void DecisionHeuristicPropagator::push() {
    // Z3 is entering a new backtracking scope - mark decision level
    decision_levels_.push_back(trail_.size());
}

void DecisionHeuristicPropagator::pop(unsigned num_scopes) {
    // Z3 is backtracking - undo changes for each scope
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (!decision_levels_.empty()) {
            // Find the start of the current decision level
            size_t level_start = decision_levels_.back();
            decision_levels_.pop_back();
            
            // Undo all trail entries added after this level
            while (trail_.size() > level_start) {
                const z3::expr& var = trail_.back();
                
                // Check if this is an action variable and remove from active/inactive sets
                if( is_action_variable(var) ) {
                    auto [action, timestep] = *variable_factory_->get_action_from_variable(var);
                    int action_node_id = action.id();
                    
                    // Remove action from both active and inactive sets using int
                    active_actions_per_timestep_[timestep].erase(action_node_id);
                    inactive_actions_per_timestep_[timestep].erase(action_node_id);
                } else {
                    // For reification variables, assign it as UNDEF (no decision has been taken on it)
                    auto [condition, timestep] = *get_condition_from_reification_variable(var);
                    condition_values_per_timestep_[timestep][condition] = Z3_L_UNDEF;
                }
                trail_.pop_back();
            }
        }
    }
}

void DecisionHeuristicPropagator::fixed(z3::expr const &var, z3::expr const &value) {
    // Check if this is a reification variable for a condition
    if (is_reification_variable(var)) {
        trail_.push_back(var);
        reification_variable_assigned(var, value);
        return; // It was a reification variable, we handled it

    } else if(is_action_variable(var)) {
        trail_.push_back(var);
        // Extract action and timestep from the variable
        auto [action, timestep] = *variable_factory_->get_action_from_variable(var);
        // Get int for the action
        int action_node_id = action.id();
        
        if (value.is_true()) {
            // Print action and its condition status
            //print_action_condition_status(action, timestep);
            
            // Update active actions for this timestep using int
            active_actions_per_timestep_[timestep].insert(action_node_id);
            
            // Perform exists propagation logic
            perform_exists_propagation(action, timestep, var);
        } else {
            // Track actions assigned to false
            inactive_actions_per_timestep_[timestep].insert(action_node_id);
            return;
        }

    } else {
        return; // dunno what this is?
    }
}

void DecisionHeuristicPropagator::final() {
    // Final constraint validation check
    // TODO: we need this?
}

z3::user_propagator_base* DecisionHeuristicPropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    return nullptr;
}


void DecisionHeuristicPropagator::register_timestep_variables(int timestep) {
    const Z3VariableFactory& var_factory = *variable_factory_;
    // Create reification variables for conditions in achievers analysis
    create_condition_reification_variables(timestep);

    // For timestep 0: register nothing as there are no actions
    if (timestep == 0) return;
 
    // where shall we look for the goal conditions?
    // it is initialized as 1, and therefore timestep 0 is covered :)
    current_goal_step_ = timestep + 1;

    // For timestep t > 0: register action variables for t-1 
    if (!registered_action_vars_.contains(timestep - 1)) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            for (const auto& var_ptr : registered_action_vars_[timestep - 1]) {
                add(*var_ptr);
            }
        }
    }
}

void DecisionHeuristicPropagator::cleanup() {
    auto& stats = Stats::instance();
    stats.set("propagator.exists_total_cycles", cycle_count_);
}

void DecisionHeuristicPropagator::perform_exists_propagation(const Action& action, int timestep, const z3::expr& action_var) {
    /*
     * EXAMPLE: Cycle detection with EXISTS semantics (3-action cycle)
     * 
     * Say we have actions at timestep 1 that form a cycle:
     *   - action_A (interferes with action_B)
     *   - action_B (interferes with action_C)  
     *   - action_C (interferes with action_A)
     * 
     * The interference pattern creates a cycle: A → B → C → A
     * 
     * When all three actions become active, we detect the cycle and report 
     * conflict with ALL actions in the cycle: {action_A, action_B, action_C}
     */
    
    // Get currently active action node IDs at this timestep (including the current action)
    const std::unordered_set<int>& active_node_ids = active_actions_per_timestep_[timestep];
    
    // Check if there's a cycle among the active actions
    std::vector<int> cycle;
    if (find_cycle_in_active_actions(active_node_ids, cycle)) {
        // Increment cycle counter
        cycle_count_++;
        
        // Report conflict with all actions in the cycle
        z3::expr_vector conflict_actions(action_var.ctx());
        for (int cycle_node_id : cycle) {
            // Convert int back to Action to get the variable
            const Action* cycle_action = &problem_->action(cycle_node_id);
            z3::expr cycle_var = variable_factory_->get_action_variable(*cycle_action, timestep);
            conflict_actions.push_back(cycle_var);
        }
        conflict(conflict_actions);
    }
}

bool DecisionHeuristicPropagator::find_cycle_in_active_actions(const std::unordered_set<int>& active_node_ids, 
                                         std::vector<int>& cycle) {
    if (active_node_ids.size() < 2) return false;
    
    std::unordered_set<int> visited;
    std::unordered_set<int> recursion_stack;
    std::vector<int> path;
    
    // Lambda for DFS with inline graph building
    std::function<bool(int)> dfs = [&](int current) -> bool {
        visited.insert(current);
        recursion_stack.insert(current);
        path.push_back(current);
        
        // Check interference with other active nodes
        for (int other_node : active_node_ids) {
            if (other_node == current) continue;
            
            if (interference_analyzer_->has_interference(current, other_node)) {
                if (recursion_stack.contains(other_node)) {
                    // Found back edge - cycle detected
                    auto cycle_start = std::find(path.begin(), path.end(), other_node);
                    cycle.assign(cycle_start, path.end());
                    return true;
                }
                
                if (!visited.contains(other_node)) {
                    if (dfs(other_node)) {
                        return true;
                    }
                }
            }
        }
        
        // Backtrack
        recursion_stack.erase(current);
        path.pop_back();
        return false;
    };
    
    // Try to find cycle starting from each unvisited node
    for (int start_node : active_node_ids) {
        if (!visited.count(start_node)) {
            if (dfs(start_node)) {
                return true;
            }
        }
    }
    
    return false;
}

void DecisionHeuristicPropagator::create_condition_reification_variables(int timestep) {
    // Create reification variables and constraints for each condition at this timestep
    const auto& pre_conditions = achievers_analysis_.get_pre_conditions();
    const auto& goal_conditions = achievers_analysis_.get_goal_conditions();

    // Lambda to create reification variables for a set of conditions
    auto create_reification_vars = [this](const std::unordered_set<Expression>& conditions, int target_timestep, const std::string& type_label) {
        for (const Expression& condition : conditions) {
            // is this condition shared with the goal (and therefore created in the previous step)?
            if (type_label == "pre" && reification_vars_per_timestep_[target_timestep].contains(condition)) {
                //auto var = reification_vars_per_timestep_[target_timestep].at(condition);
                //std::cout << "[EXISTING] " << type_label << " reification variable: " << var->to_string() << " for condition: " << condition.to_string() << " at timestep " << target_timestep << std::endl;
                continue;
            }
            // convert condition to Z3 expression at this timestep using the encoder
            auto condition_z3_opt = const_cast<BaseEncoder*>(encoder_)->convert_expression_to_z3(condition, target_timestep);
            z3::expr condition_z3 = condition_z3_opt.value();
            
            // create reification variable name with counter
            reification_counter_++;
            std::string reif_var_name = "reif_" + std::to_string(reification_counter_) + "_t" + std::to_string(target_timestep);
            z3::expr reif_var = ctx().bool_const(reif_var_name.c_str());
            
            // store the reification variable as shared_ptr and a reverse lookup mapping
            auto reif_var_ptr = std::make_shared<z3::expr>(reif_var);
            reification_vars_per_timestep_[target_timestep][condition] = reif_var_ptr;
            reification_var_name_to_condition_[reif_var_name] = {condition, target_timestep};
            // for now is undefined
            condition_values_per_timestep_[target_timestep][condition] = Z3_L_UNDEF;

            // Create reification constraint: reif_var <-> condition_z3
            z3::expr reification_constraint = (reif_var == condition_z3);

            // Print the constraint and reification variable
            //std::cout << "New " << type_label << " reification variable: " << reif_var.to_string() << " for condition: " << condition.to_string() << " at timestep " << target_timestep << std::endl;
            solver_->add(reification_constraint); // Add the constraint to the main solver
            add(reif_var); // Register the reification variable to be watched by the propagator
        }
    };

    // Process preconditions at current timestep
    create_reification_vars(pre_conditions, timestep, "pre");

    // Process goal conditions at next timestep
    create_reification_vars(goal_conditions, timestep + 1, "goal");

    // consider the edge case of t==0
    if (timestep == 0){
        create_reification_vars(goal_conditions, timestep, "goal");
    }
}

void DecisionHeuristicPropagator::reification_variable_assigned(const z3::expr& ast, const z3::expr& value) {
    // This is a reification variable
    std::string var_name = ast.decl().name().str();
    auto [condition, timestep] = reification_var_name_to_condition_.at(var_name);
    
    // Update condition value tracking
    if(value.is_true()){
        condition_values_per_timestep_[timestep][condition] = Z3_L_TRUE;
    } else {
        condition_values_per_timestep_[timestep][condition] = Z3_L_FALSE;
    }
    
    // Print the condition truth value change
    //std::cout << "T" << timestep << ": " << condition.to_string() << "=" << (is_true ? "T" : "F") << std::endl;
}

bool DecisionHeuristicPropagator::is_reification_variable(const z3::expr& var) const {
    std::string var_name = var.decl().name().str();
    return reification_var_name_to_condition_.contains(var_name);
}

bool DecisionHeuristicPropagator::is_action_variable(const z3::expr& var) const {
    return variable_factory_->is_action_variable(var);
}

std::optional<std::pair<Expression, int>> DecisionHeuristicPropagator::get_condition_from_reification_variable(const z3::expr& var) const {
    std::string var_name = var.decl().name().str();
    if (reification_var_name_to_condition_.contains(var_name)) {
        return reification_var_name_to_condition_.at(var_name);
    }
    return std::nullopt;
}

bool DecisionHeuristicPropagator::has_condition_value(const Expression& condition, int timestep) const {
    assert(timestep >= 0 && condition_values_per_timestep_[timestep].contains(condition) && "Condition not tracked");
    return condition_values_per_timestep_[timestep].at(condition) != Z3_L_UNDEF;
}

Z3_lbool DecisionHeuristicPropagator::get_condition_value(const Expression& condition, int timestep) const {
    return condition_values_per_timestep_[timestep].at(condition);
}

/*
 term	A bit-vector or Boolean used for branching
 idx	If the term is a bit-vector, then an index into the bit-vector being branched on
 phase	The tentative truth-value
*/
void DecisionHeuristicPropagator::decide(z3::expr const& term, unsigned idx, bool phase) {
    //std::cout << "\n*** DecisionHeuristicPropagator::decide() called. decision term: " << term.to_string() << ", phase: " << phase << std::endl;
    auto support_result = find_support(); // Find support for unsupported goals/subgoals

    if (support_result.has_value()) {
        //std::cout << "\n*** DecisionHeuristicPropagator returned WITH VALUE ***\n" << std::endl;
        // Found an action to support a goal/subgoal
        auto [action, timestep] = *support_result;
        assert(timestep >= 0 && "Timestep should be non-negative");
        
        z3::expr action_var = variable_factory_->get_action_variable(action, timestep);
        
        //std::cout << "DECISION HEURISTIC: Suggesting action " << action.name() << "@" << timestep << " (variable: " << action_var.to_string() << ")" << std::endl;
        
        // Use Z3's next_split to suggest this action (set to true)
        next_split(action_var, 0, Z3_L_TRUE);
        //std::cout << "*** after next_split() ***\n" << std::endl;
    } else 
    {
        //std::cout << "\n*** DecisionHeuristicPropagator returned NO VALUE ***\n" << std::endl;
    }
    //std::cout << "*** End of decide() ***\n" << std::endl;
    // If no support needed, let Z3 make the decision normally (no action needed)
}

std::optional<std::pair<Action, int>> DecisionHeuristicPropagator::find_support() const {
    // Clear and initialize stack with goal conditions
    subgoal_stack_.clear();
    const auto& goal_conditions = achievers_analysis_.get_goal_conditions();
    for (const Expression& goal_condition : goal_conditions) {
        subgoal_stack_.push_back({goal_condition, current_goal_step_});
    }
    //std::cout << "Finding support for goals at step " << current_goal_step_ << " with " << subgoal_stack_.size() << " initial goals." << std::endl;
    
    // while Stack is non-empty do
    while (!subgoal_stack_.empty()) {
        // Pop l@t from the Stack
        auto [l, t] = subgoal_stack_.back();
        subgoal_stack_.pop_back();
        if (!l.is_bool_type()) continue;
        //std::cout << "Processing subgoal: " << l.to_string() << " at timestep " << t << std::endl;
        
        // t' := t - 1;
        int t_prime = t - 1;
        // found := 0;
        bool found = false;
        
        do {
            if (t_prime < 0) break; // might be unnecesary as we should start at least at timestep 1
            
            // if v(o@t') = 1 for some o in O with l in eff(o) then
            // (that is, if the value of some action achieving l at t' is true)
            const auto& achiever_actions = achievers_analysis_.get_achievers(l);
            for (const Action& o : achiever_actions) {
                int action_node_id = o.id();
                if (active_actions_per_timestep_.at(t_prime).contains(action_node_id)) {
                    // For all l' in prec(o) do push l'@t' into the Stack
                    //std::cout << "Found active achiever: action " << o.name() << " is active at T" << t_prime << ", adding its preconditions to the stack." << std::endl;
                    for (const Expression& precond : achievers_analysis_.get_preconditions(o)) {
                        subgoal_stack_.push_back({precond, t_prime});
                        //std::cout << "  Added precondition as subgoal: " << precond.to_string() << " at timestep " << t_prime << std::endl;
                    }
                    found = true;
                    break; // early exit from the loop
                }
            }
            
            // Check if v(l@t') = 0 (literal is false at this timestep)
            if (has_condition_value(l, t_prime) && get_condition_value(l, t_prime) == Z3_L_FALSE) {
                // Return any o in O such that l in eff(o) and v(o@t') != 0
                for (const Action& o : achiever_actions) {
                    int action_node_id = o.id();
                    // Check if action is unassigned or true
                    if (
                        (!active_actions_per_timestep_.at(t_prime).contains(action_node_id) &&
                        !inactive_actions_per_timestep_.at(t_prime).contains(action_node_id))
                        ||
                        (active_actions_per_timestep_.at(t_prime).contains(action_node_id)) 
                     ) {
                        // Action is truly unassigned, so we can suggest it
                        //std::cout << "Found unassigned achiever action " << o.name() << " at T" << t_prime << " for condition " << l.to_string() << std::endl;
                        return std::make_pair(o, t_prime);
                    }
                }
            }
            t_prime = t_prime - 1;
        } while (!found && t_prime >= 0);
    }
    //std::cout << "No support action found for any subgoal." << std::endl;
    return std::nullopt;
}

void DecisionHeuristicPropagator::print_condition_values() const {
    std::cout << "=== Condition Values Summary ===" << std::endl;
    
    bool has_any_values = false;
    for (int timestep = 0; timestep < static_cast<int>(condition_values_per_timestep_.size()); ++timestep) {
        const auto& timestep_map = condition_values_per_timestep_[timestep];
        if (!timestep_map.empty()) {
            has_any_values = true;
            std::cout << "T" << timestep << ": ";
            
            for (const auto& [condition, value] : timestep_map) {
                if(value == Z3_L_TRUE) std::cout << condition.to_string() << "=T, " << std::endl;
                if(value == Z3_L_FALSE) std::cout << condition.to_string() << "=F, " << std::endl;
                if(value == Z3_L_UNDEF) std::cout << condition.to_string() << "=U, " << std::endl;
            }
        }
    }
    
    if (!has_any_values) {
        std::cout << "No condition values assigned yet." << std::endl;
    }
    std::cout << "===============================" << std::endl;
}

void DecisionHeuristicPropagator::print_action_condition_status(const Action& action, int timestep) const {
    // Now that achievers analysis uses value-based maps, we can call get_preconditions directly
    const auto& preconditions = achievers_analysis_.get_preconditions(action);
    bool has_relevant_conditions = false;
    std::cout << "Fixed action " << action.name() << " at T" << timestep << " - Conditions("<< preconditions.size() <<"): ";

    for (const Expression& condition : preconditions) {
        std::string condition_str = condition.to_string();
        std::string value = "U";
        if (has_condition_value(condition, timestep)) {
            if (has_relevant_conditions) std::cout << ", ";
            value = get_condition_value(condition, timestep) ? "T" : "F";
            has_relevant_conditions = true;
        }
        std::cout << condition.to_string() << "=" << value << ", ";
    }

    std::cout << std::endl << std::endl;
}

} // namespace rantanplan