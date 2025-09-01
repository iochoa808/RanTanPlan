#include "decision_heuristic_propagator.h"
#include "../../abstraction/achievers_analysis.h"
#include "../../config/config.h"
#include "../../util/memory_tracker.h"
#include "../../util/stats.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analysis.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <functional>

namespace planmt {

DecisionHeuristicPropagator::DecisionHeuristicPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder)
    : z3::user_propagator_base(&solver), solver_(&solver), problem_(&problem), encoder_(&encoder),
     variable_factory_(&encoder.get_variable_factory()),
     parallelism_strategy_(encoder.get_parallelism_strategy()),
     interference_analyzer_(parallelism_strategy_->get_interference_analyzer()), 
     achievers_analysis_(problem),
     cycle_count_(0),
     reification_counter_(0) {

    // Define callbacks for the user propagator
    register_fixed();
    register_final();
    register_decide();

    // pre-allocate vector for up to max_steps timesteps
    // and pre-reserve capacity in each timestep map based on number of conditions
    reification_vars_per_timestep_.resize(Config::instance().planner.max_steps);
    condition_values_per_timestep_.resize(Config::instance().planner.max_steps);
    auto all_conditions = achievers_analysis_.get_all_conditions();
    for (auto& timestep_map : reification_vars_per_timestep_) {
        timestep_map.reserve(all_conditions.size());
    }
    for (auto& timestep_map : condition_values_per_timestep_) {
        timestep_map.reserve(all_conditions.size());
    }

    // Set Z3 option to persist clauses for user propagator based on config
    solver.set("smt.up.persist_clauses", Config::instance().propagators.persist_clauses);
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
                
                // Check if this is an action variable and remove from active set
                if( is_action_variable(var) ) {
                    auto [action, timestep] = *variable_factory_->get_action_from_variable(var);
                    Graph::NodeId action_node_id = interference_analyzer_->get_action_node_id(action);
                    
                    // Remove action from active set using NodeId
                    active_actions_per_timestep_[timestep].erase(action_node_id);
                } else {
                    // For reification variables, remove from condition values tracking
                    auto [condition, timestep] = *get_condition_from_reification_variable(var);
                    condition_values_per_timestep_[timestep].erase(condition);
                }
                trail_.pop_back();
            }
        }
    }
}

void DecisionHeuristicPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    // Check if this is a reification variable for a condition
    if (is_reification_variable(ast)) {
        trail_.push_back(ast);
        reification_variable_assigned(ast, value);
        return; // It was a reification variable, we handled it

    } else if(is_action_variable(ast)) {
        trail_.push_back(ast);
        // From now on we only care about action variables being set to true
        if (!value.is_true()) return;
        // Extract action and timestep from the variable
        auto [action, timestep] = *variable_factory_->get_action_from_variable(ast);
        // Get NodeId for the action
        Graph::NodeId action_node_id = interference_analyzer_->get_action_node_id(action);
        
        // Print action and its condition status
        print_action_condition_status(action, timestep);
        
        // Update active actions for this timestep using NodeId
        active_actions_per_timestep_[timestep].insert(action_node_id);
        
        // Perform exists propagation logic
        perform_exists_propagation(action, timestep, ast);

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
    
    // For timestep t > 0: register action variables for t-1 
    if (!registered_action_vars_.contains(timestep - 1)) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            for (const auto& var : registered_action_vars_[timestep - 1]) {
                add(var);
            }
        }
    }
}

PropagatorType DecisionHeuristicPropagator::get_type() const {
    return PropagatorType::HEURISTIC;
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
    const std::unordered_set<Graph::NodeId>& active_node_ids = active_actions_per_timestep_[timestep];
    
    // Check if there's a cycle among the active actions
    std::vector<Graph::NodeId> cycle;
    if (find_cycle_in_active_actions(active_node_ids, cycle)) {
        // Increment cycle counter
        cycle_count_++;
        
        // Report conflict with all actions in the cycle
        z3::expr_vector conflict_actions(action_var.ctx());
        for (Graph::NodeId cycle_node_id : cycle) {
            // Convert NodeId back to Action to get the variable
            const Action* cycle_action = interference_analyzer_->get_action_from_node_id(cycle_node_id);
            z3::expr cycle_var = variable_factory_->get_action_variable(*cycle_action, timestep);
            conflict_actions.push_back(cycle_var);
        }
        conflict(conflict_actions);
    }
}

bool DecisionHeuristicPropagator::find_cycle_in_active_actions(const std::unordered_set<Graph::NodeId>& active_node_ids, 
                                         std::vector<Graph::NodeId>& cycle) {
    if (active_node_ids.size() < 2) return false;
    
    std::unordered_set<Graph::NodeId> visited;
    std::unordered_set<Graph::NodeId> recursion_stack;
    std::vector<Graph::NodeId> path;
    
    // Lambda for DFS with inline graph building
    std::function<bool(Graph::NodeId)> dfs = [&](Graph::NodeId current) -> bool {
        visited.insert(current);
        recursion_stack.insert(current);
        path.push_back(current);
        
        // Check interference with other active nodes
        for (Graph::NodeId other_node : active_node_ids) {
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
    for (Graph::NodeId start_node : active_node_ids) {
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
    auto all_conditions = achievers_analysis_.get_all_conditions();
    for (const Expression& condition : all_conditions) {
        // convert condition to Z3 expression at this timestep using the encoder
        auto condition_z3_opt = const_cast<BaseEncoder*>(encoder_)->convert_expression_to_z3(condition, timestep);
        z3::expr condition_z3 = condition_z3_opt.value();
        
        // create reification variable name with counter
        reification_counter_++;
        std::string reif_var_name = "reif_" + std::to_string(reification_counter_) + "_t" + std::to_string(timestep);
        z3::expr reif_var = ctx().bool_const(reif_var_name.c_str());
        
        // store the reification variable as shared_ptr
        // and a reverse lookup mapping
        auto reif_var_ptr = std::make_shared<z3::expr>(reif_var);
        reification_vars_per_timestep_[timestep][condition] = reif_var_ptr;
        reification_var_name_to_condition_[reif_var_name] = {condition, timestep};
        
        // Create reification constraint: reif_var <-> condition_z3
        z3::expr reification_constraint = (reif_var == condition_z3);

        // Print the constraint and reification variable
        std::cout << "New reification variable: " << reif_var.to_string() << " for condition: " << condition.to_string() << " at timestep " << timestep << std::endl;
        solver_->add(reification_constraint); // Add the constraint to the main solver
        add(reif_var); // Register the reification variable to be watched by the propagator
    }
}

void DecisionHeuristicPropagator::reification_variable_assigned(const z3::expr& ast, const z3::expr& value) {
    // This is a reification variable
    std::string var_name = ast.decl().name().str();
    auto [condition, timestep] = reification_var_name_to_condition_.at(var_name);
    bool is_true = value.is_true();
    
    // Update condition value tracking
    condition_values_per_timestep_[timestep][condition] = is_true;
    
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
    const auto& timestep_map = condition_values_per_timestep_[timestep];
    return timestep_map.contains(condition);
}

bool DecisionHeuristicPropagator::get_condition_value(const Expression& condition, int timestep) const {
    const auto& timestep_map = condition_values_per_timestep_[timestep];
    if (timestep_map.contains(condition)) return timestep_map.at(condition);
    return false;
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
                std::cout << condition.to_string() << "=" << (value ? "T" : "F") << ", ";
            }
            std::cout << std::endl;
        }
    }
    
    if (!has_any_values) {
        std::cout << "No condition values assigned yet." << std::endl;
    }
    std::cout << "===============================" << std::endl;
}

void DecisionHeuristicPropagator::print_action_condition_status(const Action& action, int timestep) const {
    // Now that achievers analysis uses value-based maps, we can call get_preconditions directly
    auto preconditions = achievers_analysis_.get_preconditions(action);
    bool has_relevant_conditions = false;
    std::cout << "Action " << action.name() << " at T" << timestep << " - Conditions("<< preconditions.size() <<"): ";

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
    
    //if (!has_relevant_conditions) {
    //    std::cout << std::endl << std::endl;
    //    print_condition_values();
    //}

    std::cout << std::endl << std::endl;
}

/*
 term	A bit-vector or Boolean used for branching
 idx	If the term is a bit-vector, then an index into the bit-vector being branched on
 phase	The tentative truth-value
*/
void DecisionHeuristicPropagator::decide(z3::expr const& term, unsigned idx, bool phase) {
    // Print the variable and phase that Z3 is about to decide on
    std::cout << "DecisionHeuristic decide callback: var=" << term << ", value=" << phase << std::endl;

    // Get candidates for decision making
    auto candidates = get_decision_candidates();
    if (candidates.empty()) return;
    
    z3::expr selected = select_next_split(candidates);
    //std::cout << "DecisionHeuristic overriding with: " << selected.to_string() << std::endl;
    // Use Z3's next_split to influence the decision
    //next_split(selected, 0, Z3_L_TRUE); // Phase Z3_L_TRUE = try setting to true first
}

std::vector<z3::expr> DecisionHeuristicPropagator::get_decision_candidates() const {
    std::vector<z3::expr> candidates;
    
    // Collect all registered action variables as candidates
    for (const auto& [timestep, vars] : registered_action_vars_) {
        for (const auto& var : vars) {
            candidates.push_back(var);
        }
    }
    
    return candidates;
}

z3::expr DecisionHeuristicPropagator::select_next_split(const std::vector<z3::expr>& candidates) const {
    // For now, just return the first candidate
    // TODO: Add actual heuristics here based on goal distance, action priorities, etc.
    // The caller already checks if candidates is empty
    return candidates[0];
}

} // namespace planmt