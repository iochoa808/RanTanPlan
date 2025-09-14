#include "arpg.h"
#include "../config/config.h"
#include "../util/memory_tracker.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>

namespace planmt {

ARPG::ARPG(const Problem& problem) 
    : problem_(problem), goal_reached_(false) {
    auto& config = Config::instance();
    if (config.is_info()) {
        double current_memory = MemoryTracker::instance().get_current_memory_mb();
        std::cout << "[ARPG] Starting, memory=" << current_memory << "MB" << std::endl;
    }
    create_supporters_from_actions();
}

bool ARPG::construct_graph() {
    // Algorithm 1: Asymptotic Relaxed Planning Graph (ARPG)
    auto& config = Config::instance();
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Initialize with the initial relaxed state (s+ = s0+)
    RelaxedState initial_state = create_initial_relaxed_state();
    interval_layers_.emplace_back(initial_state, 0);
    
    
    std::unordered_set<size_t> used_supporters;
    int layer_num = 0;
    
    while (true) {
        const RelaxedState& current_state = interval_layers_.back().state;
        
        // Check if goal is satisfied
        if (check_goal_satisfaction(current_state)) {
            goal_reached_ = true;
            std::cout << "[ARPG] reached goal at layer " << layer_num << std::endl;
            //std::cout << "Final state: " << current_state.to_string() << std::endl;
            break;
        }
        
        // Find applicable supporters that haven't been used
        auto applicable = find_applicable_supporters(current_state, used_supporters);
        
        // If no new supporters, terminate
        if (applicable.empty()) {
            std::cout << "[ARPG] No more applicable supporters at layer " << layer_num << std::endl;
            break;
        }
        
        // Create supporter layer
        SupporterLayer supporter_layer(layer_num);
        supporter_layer.applicable_supporters = applicable;
        supporter_layers_.push_back(supporter_layer);
        
        // Mark supporters as used
        for (size_t i = 0; i < supporters_.size(); ++i) {
            for (const auto& supp : applicable) {
                if (supporters_[i] == supp) {
                    used_supporters.insert(i);
                }
            }
        }
        
        // Apply supporters to create next interval layer
        RelaxedState next_state = apply_supporters(current_state, applicable);
        
        
        interval_layers_.emplace_back(next_state, layer_num + 1);
        
        layer_num++;
        
        // Safety check to prevent infinite loops
        if (layer_num > 100) {
            std::cout << "[ARPG] Maximum layer limit reached" << std::endl;
            break;
        }
    }
    
    // Print timing and memory info for completion
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration<double>(end_time - start_time).count();
    if (config.is_info()) {
        double current_memory = MemoryTracker::instance().get_current_memory_mb();
        std::cout << "[ARPG] construction took: time=" << total_time << "s, memory=" << current_memory << "MB";
        std::cout << std::endl;
        //print_construction_steps();
    }
    
    return goal_reached_;
}

void ARPG::create_supporters_from_actions() {
    supporters_.clear();
    
    for (const auto& action : problem_.actions()) {
        for (const auto& effect : action.effects()) {
            auto action_supporters = create_supporters_for_effect(action, effect);
            supporters_.insert(supporters_.end(), action_supporters.begin(), action_supporters.end());
        }
    }
    
    //std::cout << "Created " << supporters_.size() << " supporters from " << problem_.actions().size() << " actions" << std::endl;
}

std::vector<std::shared_ptr<Supporter>> ARPG::create_supporters_for_effect(
    const Action& action, const Effect& effect) {
    
    std::vector<std::shared_ptr<Supporter>> result;
    
    // Extract variable name from the effect
    std::string var_name = effect.fluent().to_string();
    if (var_name.empty()) {
        return result;
    }
    
    
    // Handle ALL effects to ensure ARPG is at least as informative as RPG
    
    // Distinguish between boolean predicates and numeric functions using Expression type info
    // Check if the fluent being affected has boolean type
    bool is_boolean_fluent = effect.fluent().is_bool_type();

    if (is_boolean_fluent || (!effect.effect_expression().is_increase() && !effect.effect_expression().is_decrease() && !effect.effect_expression().is_assign())) {
        // This is a propositional effect (boolean predicate or non-numeric operation)
        // In relaxed planning, we ignore delete effects and only consider add effects
        // This is fundamental to RPG construction - every positive effect creates a supporter

        auto add_supporter = std::make_shared<Supporter>(
            action.name() + "_add_" + var_name, var_name, &action);
        add_supporter->add_precondition(action.precondition());
        add_supporter->set_boolean_add_effect();
        result.push_back(add_supporter);

        return result;
    }
    
    // For numeric variables - simplified ARPG approach
    if (effect.effect_expression().is_increase()) {
        // Increase effect: create single supporter with positive infinity effect
        auto supporter = std::make_shared<Supporter>(
            action.name() + "_" + var_name + "_increase", var_name, &action);
        supporter->add_precondition(action.precondition());
        supporter->set_positive_infinity_effect();
        result.push_back(supporter);
        
    } else if (effect.effect_expression().is_decrease()) {
        // Decrease effect: create single supporter with negative infinity effect  
        auto supporter = std::make_shared<Supporter>(
            action.name() + "_" + var_name + "_decrease", var_name, &action);
        supporter->add_precondition(action.precondition());
        supporter->set_negative_infinity_effect();
        result.push_back(supporter);
        
    } else if (effect.effect_expression().is_assign()) {
        // For assignments in relaxed planning, we create a supporter that can achieve any positive value
        // This handles both constant assignments and variable assignments optimistically
        auto assignment_supporter = std::make_shared<Supporter>(
            action.name() + "_assign_" + var_name, var_name, &action);
        assignment_supporter->add_precondition(action.precondition());
        assignment_supporter->set_positive_infinity_effect(); // Optimistic: can assign any value
        result.push_back(assignment_supporter);
    } else {
        // Fallback: create a general supporter for any unhandled effect types
        // This ensures we don't miss any effects that should contribute to the RPG baseline
        auto general_supporter = std::make_shared<Supporter>(
            action.name() + "_effect_" + var_name, var_name, &action);
        general_supporter->add_precondition(action.precondition());
        general_supporter->set_boolean_add_effect(); // Default to boolean add
        result.push_back(general_supporter);
    }
    
    return result;
}

std::vector<std::shared_ptr<Supporter>> ARPG::find_applicable_supporters(
    const RelaxedState& state, const std::unordered_set<size_t>& used_supporters) const {
    
    std::vector<std::shared_ptr<Supporter>> applicable;
    
    for (size_t i = 0; i < supporters_.size(); ++i) {
        if (used_supporters.find(i) != used_supporters.end()) {
            continue; // Already used
        }
        
        if (supporters_[i]->is_applicable(state)) {
            applicable.push_back(supporters_[i]);
        }
    }
    
    return applicable;
}

RelaxedState ARPG::apply_supporters(const RelaxedState& current_state,
                                    const std::vector<std::shared_ptr<Supporter>>& supporters) const {
    RelaxedState next_state = current_state; // Copy current state
    
    // Apply each supporter's effect
    for (const auto& supporter : supporters) {
        supporter->apply_effect(next_state);
    }
    
    return next_state;
}

bool ARPG::check_goal_satisfaction(const RelaxedState& state) const {
    // Check if all goal conditions are satisfied in the relaxed state
    //std::cout << "[ARPG] Checking goal satisfaction with " << problem_.goals().size() << " goal conditions:" << std::endl;

    for (const auto& goal_condition : problem_.goals()) {
        bool satisfied = state.satisfies_condition(goal_condition.goal_expression());
        //std::cout << "  Goal: " << goal_condition.goal_expression().to_string()
        //          << " -> " << (satisfied ? "SATISFIED" : "NOT SATISFIED") << std::endl;
        if (!satisfied) {
            return false;
        }
    }
    //std::cout << "[ARPG] All goals satisfied!" << std::endl;
    return true;
}

RelaxedState ARPG::create_initial_relaxed_state() const {
    RelaxedState initial_state;
    
    // Process all initial state assignments
    for (const auto& assignment : problem_.initial_state()) {
        std::string var_name = assignment.fluent().to_string();
        
        // Handle numeric assignments
        if (assignment.value().is_constant() && assignment.value().is_atom()) {
            if (assignment.value().value().is_real()) {
                // Real numeric value
                double value = assignment.value().value().real().to_double();
                initial_state.set_variable(var_name, Interval(value));
            } else if (assignment.value().value().is_integer()) {
                // Integer numeric value
                double value = static_cast<double>(assignment.value().value().integer());
                initial_state.set_variable(var_name, Interval(value));
            } else if (assignment.value().value().is_boolean()) {
                // Boolean assignment
                if (assignment.value().value().boolean()) {
                    initial_state.add_proposition(var_name);
                }
            }
        } else {
            // For non-constant assignments, treat as propositions
            // In classical planning, non-false propositions are considered true in initial state
            initial_state.add_proposition(var_name);
        }
    }
    
    // Note: We don't need to initialize any additional numeric variables
    // All real numeric variables should be explicitly set from initial state assignments
    // The get_numeric_variable_names() method returns ungrounded fluent names which are not real variables
    
    return initial_state;
}


bool ARPG::is_goal_reachable() const {
    return goal_reached_;
}

const RelaxedState& ARPG::get_final_state() const {
    if (!interval_layers_.empty()) {
        return interval_layers_.back().state;
    }
    static RelaxedState empty_state;
    return empty_state;
}

std::unordered_map<Expression, Interval> ARPG::get_state_variable_bounds() const {
    std::unordered_map<Expression, Interval> bounds_map;

    if (interval_layers_.empty()) {
        return bounds_map;
    }

    const RelaxedState& final_state = interval_layers_.back().state;
    //std::cout << "[ARPG] Final state (layer " << (interval_layers_.size() - 1) << ") variables:" << std::endl;

    // Print each numeric variable and its bounds
    //auto var_names = final_state.get_variable_names();
    //for (const auto& var : var_names) {
    //    auto interval_opt = final_state.get_variable(var);
    //    if (interval_opt.has_value()) {
    //        std::cout << "  " << var << ": " << interval_opt->to_string() << std::endl;
    //    }
    //}

    // Print propositions
    //auto props = final_state.get_true_propositions();
    //if (!props.empty()) {
    //    std::cout << "  Propositions: ";
    //    bool first = true;
    //    for (const auto& prop : props) {
    //        if (!first) std::cout << ", ";
    //        std::cout << prop;
    //        first = false;
    //    }
    //    std::cout << std::endl;
    //}
    
    // Collect all state variable expressions from the problem and match them to final state
    std::unordered_set<Expression> all_state_vars;

    // Add from initial state
    for (const auto& assignment : problem_.initial_state()) {
        all_state_vars.insert(assignment.fluent());
    }

    // Add from action effects
    for (const auto& action : problem_.actions()) {
        for (const auto& effect : action.effects()) {
            all_state_vars.insert(effect.fluent());
        }
    }

    // For each state variable expression, check if we have bounds for it in final state
    for (const Expression& state_var : all_state_vars) {
        std::string var_name = state_var.to_string();
        auto interval_opt = final_state.get_variable(var_name);
        if (interval_opt.has_value()) {
            bounds_map[state_var] = interval_opt.value();
        }
    }
    
    return bounds_map;
}

void ARPG::print_construction_steps() const {
    std::cout << "\n=== ARPG Construction Steps ===" << std::endl;
    std::cout << "Total supporters created: " << supporters_.size() << std::endl;
    
    for (const auto& supporter : supporters_) {
        std::cout << "  " << supporter->to_string() << std::endl;
    }
    
    std::cout << "\n=== Layer-by-Layer Construction ===" << std::endl;
    
    for (size_t i = 0; i < interval_layers_.size(); ++i) {
        const auto& interval_layer = interval_layers_[i];
        std::cout << "\nInterval Layer " << interval_layer.layer_number << ":" << std::endl;
        std::cout << interval_layer.state.to_string() << std::endl;
        
        if (i < supporter_layers_.size()) {
            const auto& supporter_layer = supporter_layers_[i];
            std::cout << "\nSupporter Layer " << supporter_layer.layer_number 
                      << " (" << supporter_layer.applicable_supporters.size() 
                      << " supporters applied):" << std::endl;
            
            for (const auto& supporter : supporter_layer.applicable_supporters) {
                std::cout << "  Applied: " << supporter->to_string() << std::endl;
            }
        }
    }
    
    std::cout << "\nGoal reachable: " << (goal_reached_ ? "YES" : "NO") << std::endl;
}

void ARPG::export_dot_file(const std::string& filename) const {
    std::ofstream dot_file(filename);
    if (!dot_file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing" << std::endl;
        return;
    }
    
    dot_file << "digraph ARPG {" << std::endl;
    dot_file << "  rankdir=TB;" << std::endl;
    dot_file << "  compound=true;" << std::endl;
    dot_file << std::endl;
    
    // Create subgraphs for each layer to enforce proper ranking
    for (size_t i = 0; i < interval_layers_.size(); ++i) {
        const auto& interval_layer = interval_layers_[i];
        const auto& state = interval_layer.state;
        
        // State layer (propositions and fluents as individual nodes)
        dot_file << "  subgraph cluster_state_" << i << " {" << std::endl;
        dot_file << "    rank=same;" << std::endl;
        dot_file << "    label=\"State Layer " << i;
        if (i == 0) {
            dot_file << " (Initial)";
        }
        dot_file << "\";" << std::endl;
        dot_file << "    style=dashed;" << std::endl;
        dot_file << "    color=blue;" << std::endl;
        
        // Individual nodes for each proposition
        auto props = state.get_true_propositions();
        for (const auto& prop : props) {
            std::string prop_id = "S" + std::to_string(i) + "_" + std::to_string(std::hash<std::string>{}(prop) % 10000);
            std::string clean_prop = prop;
            // Escape quotes and newlines for DOT
            size_t pos = 0;
            while ((pos = clean_prop.find("\"", pos)) != std::string::npos) {
                clean_prop.replace(pos, 1, "\\\"");
                pos += 2;
            }
            
            dot_file << "    " << prop_id << " [label=\"" << clean_prop 
                     << "\", shape=ellipse, style=filled, fillcolor=lightcyan];" << std::endl;
        }
        
        // Individual nodes for each numeric variable
        auto var_names = state.get_variable_names();
        for (const auto& var : var_names) {
            auto interval_opt = state.get_variable(var);
            if (interval_opt.has_value()) {
                std::string var_id = "V" + std::to_string(i) + "_" + std::to_string(std::hash<std::string>{}(var) % 10000);
                std::string clean_var = var;
                // Escape quotes
                size_t pos = 0;
                while ((pos = clean_var.find("\"", pos)) != std::string::npos) {
                    clean_var.replace(pos, 1, "\\\"");
                    pos += 2;
                }
                
                dot_file << "    " << var_id << " [label=\"" << clean_var 
                         << "\\n" << interval_opt->to_string() 
                         << "\", shape=box, style=filled, fillcolor=lightblue];" << std::endl;
            }
        }
        
        dot_file << "  }" << std::endl;
        dot_file << std::endl;
        
        // Action/Supporter layer
        if (i < supporter_layers_.size()) {
            const auto& supporter_layer = supporter_layers_[i];
            
            dot_file << "  subgraph cluster_action_" << i << " {" << std::endl;
            dot_file << "    rank=same;" << std::endl;
            dot_file << "    label=\"Action Layer " << i << "\";" << std::endl;
            dot_file << "    style=dashed;" << std::endl;
            dot_file << "    color=green;" << std::endl;
            
            // Individual nodes for each applicable supporter
            for (size_t j = 0; j < supporter_layer.applicable_supporters.size(); ++j) {
                const auto& supporter = supporter_layer.applicable_supporters[j];
                std::string supp_id = "A" + std::to_string(i) + "_" + std::to_string(j);
                std::string supp_name = supporter->name();
                
                // Truncate very long names and escape quotes
                if (supp_name.length() > 25) {
                    supp_name = supp_name.substr(0, 22) + "...";
                }
                size_t pos = 0;
                while ((pos = supp_name.find("\"", pos)) != std::string::npos) {
                    supp_name.replace(pos, 1, "\\\"");
                    pos += 2;
                }
                
                dot_file << "    " << supp_id << " [label=\"" << supp_name 
                         << "\", shape=rectangle, style=filled, fillcolor=lightgreen];" << std::endl;
            }
            
            dot_file << "  }" << std::endl;
            dot_file << std::endl;
        }
    }
    
    // Add edges showing how supporters connect states
    // For simplicity, we'll connect each supporter to the variables/propositions it affects
    for (size_t i = 0; i < supporter_layers_.size(); ++i) {
        const auto& supporter_layer = supporter_layers_[i];
        const auto& current_state = interval_layers_[i].state;
        const auto& next_state = interval_layers_[i + 1].state;
        
        // For each supporter in this layer
        for (size_t j = 0; j < supporter_layer.applicable_supporters.size(); ++j) {
            const auto& supporter = supporter_layer.applicable_supporters[j];
            std::string supp_id = "A" + std::to_string(i) + "_" + std::to_string(j);
            
            // Connect from preconditions in current state to supporter
            // (Simplified: connect from current state layer to supporter)
            auto current_props = current_state.get_true_propositions();
            if (!current_props.empty()) {
                std::string first_prop_id = "S" + std::to_string(i) + "_" + std::to_string(std::hash<std::string>{}(*current_props.begin()) % 10000);
                dot_file << "  " << first_prop_id << " -> " << supp_id 
                         << " [style=dotted, color=gray];" << std::endl;
            }
            
            // Connect from supporter to effects in next state
            // (Simplified: connect to the variable this supporter affects)
            std::string var_name = supporter->affected_variable();
            std::string next_var_id = "V" + std::to_string(i + 1) + "_" + std::to_string(std::hash<std::string>{}(var_name) % 10000);
            
            // Check if it's a boolean effect
            if (supporter->effect_type() == Supporter::EffectType::BOOLEAN_ADD) {
                next_var_id = "S" + std::to_string(i + 1) + "_" + std::to_string(std::hash<std::string>{}(var_name) % 10000);
            }
            
            dot_file << "  " << supp_id << " -> " << next_var_id 
                     << " [color=red];" << std::endl;
        }
    }
    
    // Add goal indication
    if (goal_reached_) {
        dot_file << std::endl;
        dot_file << "  goal [label=\"GOAL\\nREACHED\", shape=diamond, style=filled, fillcolor=gold];" << std::endl;
        
        // Connect from final state layer to goal
        if (!interval_layers_.empty()) {
            size_t final_layer = interval_layers_.size() - 1;
            const auto& final_state = interval_layers_[final_layer].state;
            auto final_props = final_state.get_true_propositions();
            if (!final_props.empty()) {
                std::string final_prop_id = "S" + std::to_string(final_layer) + "_" + std::to_string(std::hash<std::string>{}(*final_props.begin()) % 10000);
                dot_file << "  " << final_prop_id << " -> goal [color=gold, style=bold];" << std::endl;
            }
        }
    }
    
    dot_file << "}" << std::endl;
    dot_file.close();
    
    std::cout << "ARPG exported to DOT file: " << filename << std::endl;
    std::cout << "To generate image: dot -Tpng " << filename << " -o arpg.png" << std::endl;
}

std::string ARPG::to_string() const {
    std::string result = "ARPG {\n";
    result += "  Layers: " + std::to_string(interval_layers_.size()) + "\n";
    result += "  Supporters: " + std::to_string(supporters_.size()) + "\n";
    result += "  Goal reachable: " + std::string(goal_reached_ ? "true" : "false") + "\n";
    result += "}";
    return result;
}

} // namespace planmt