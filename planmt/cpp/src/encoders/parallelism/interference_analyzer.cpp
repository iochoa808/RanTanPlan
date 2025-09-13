#include "interference_analyzer.h"
#include "../../util/memory_tracker.h"
#include "../../config/config.h"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <fstream> // For outputting the graph to a DOT file
#include <chrono>

namespace planmt {

InterferenceAnalyzer::InterferenceAnalyzer(const Problem& problem) {
    initialize(problem);
}

void InterferenceAnalyzer::initialize(const Problem& problem) {
    problem_ = &problem;
    
    // Clear any existing data
    action_analysis_.clear();
    interference_graph_ = Graph();

    // Create nodes for each action and analyze them
    for (const Action& action : problem.actions()) {
        interference_graph_.add_node();

        // Analyze this action and store the results
        action_analysis_[action] = analyze_action(action);
    }

    // Report memory usage after action analysis
    double current_memory = MemoryTracker::instance().get_current_memory_mb();
    std::cout << "InterferenceAnalyzer initialized with " << problem.actions().size() 
              << " actions and indexed their preconditions and effects. "
              << "Memory: " << current_memory << " MB" << std::endl;
    // Print the action analysis results
    build_interference_graph();
    // DEBUG
    //print_action_analysis();
    //output_interference_graph_dot();
}

void InterferenceAnalyzer::build_interference_graph() {
    if (!problem_) {
        std::cout << "Error: InterferenceAnalyzer not initialized with a problem" << std::endl;
        return;
    }
    
    std::cout << "Building interference graph..." << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    analyze_action_conflicts();
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time);
    
    // Report memory usage after O(n²) graph building
    double current_memory = MemoryTracker::instance().get_current_memory_mb();
    std::cout << "Interference graph built with " << interference_graph_.num_nodes() 
              << " nodes and " << interference_graph_.num_edges() << " edges. "
              << "Time: " << duration.count() << "s, Memory: " << current_memory << " MB" << std::endl;
}

void InterferenceAnalyzer::analyze_action_conflicts() {
    const auto& actions = problem_->actions();
    
    // Expensive O(n²) preprocessing: analyze all pairs of actions for conflicts
    // Results are cached in the interference graph for fast lookup during execution
    for (size_t i = 0; i < actions.size(); ++i) {
        for (size_t j = 0; j < actions.size(); ++j) {
            if (i != j) {  // Don't check action against itself
                const Action& action1 = actions[i];
                const Action& action2 = actions[j];
                
                if (actions_interfere(action1, action2)) {
                    // Cache directional interference: action1 interferes with action2
                    int node1 = action1.id();
                    int node2 = action2.id();
                    interference_graph_.add_edge(node1, node2);
                }
            }
        }
    }
}

bool InterferenceAnalyzer::actions_interfere(const Action& a1, const Action& a2) const {
    // Get analysis for both actions
    auto it1 = action_analysis_.find(a1);
    auto it2 = action_analysis_.find(a2);
    
    if (it1 == action_analysis_.end() || it2 == action_analysis_.end()) {
        return false;
    }
    
    const ActionAnalysis& analysis_a1 = it1->second;
    const ActionAnalysis& analysis_a2 = it2->second;
    
    // Helper lambda to check if two sets have non-empty intersection
    auto has_intersection = [](const std::unordered_set<Expression>& set1, 
                              const std::unordered_set<Expression>& set2) -> bool {
        for (const auto& elem : set1) {
            if (set2.find(elem) != set2.end()) {
                return true;
            }
        }
        return false;
    };
    
    // 1. Check if effects of a1 can prevent execution of a2
    // Effects of a1 interfere with preconditions of a2
    if (has_intersection(analysis_a1.positive_boolean_effects, analysis_a2.negative_boolean_preconditions) ||
        has_intersection(analysis_a1.negative_boolean_effects, analysis_a2.positive_boolean_preconditions) ||
        has_intersection(analysis_a1.numeric_effects, analysis_a2.numeric_preconditions)) {
        return true;
    }
    
    // 2. Check if effects of a1 can affect the conditions of a2's conditional effects
    if (has_intersection(analysis_a1.all_effects, analysis_a2.conditional_effect_fluents)) {
        return true;
    }

    // 3. Check if effects of a1 can affect the RHS of a2's numeric effects
    if (has_intersection(analysis_a1.all_effects, analysis_a2.numeric_effect_dependencies)) {
        return true;
    }

    // 4. Check if effects of a1 and a2 interfere (same fluents modified differently or both modified)
    if (has_intersection(analysis_a1.positive_boolean_effects, analysis_a2.negative_boolean_effects) ||
        has_intersection(analysis_a1.negative_boolean_effects, analysis_a2.positive_boolean_effects) ||
        has_intersection(analysis_a1.numeric_effects, analysis_a2.numeric_effects)) {
        return true;
    }
    
    return false;
}

bool InterferenceAnalyzer::has_interference(const Action& a1, const Action& a2) const {

    // Fast O(1) lookup in the pre-built interference graph using action IDs
    return interference_graph_.has_edge(a1.id(), a2.id());
}

InterferenceAnalyzer::ActionAnalysis InterferenceAnalyzer::analyze_action(const Action& action) const {
    ActionAnalysis analysis;
    // Analyze preconditions and effects
    analyze_preconditions(action, analysis);
    analyze_effects(action, analysis);
    return analysis;
}

void InterferenceAnalyzer::analyze_preconditions(const Action& action, ActionAnalysis& analysis) const {
    if (!action.has_precondition()) {
        return;
    }
    
    // Use the fluent polarity collector to analyze preconditions and split by polarity
    FluentPolarityCollector collector;
    collector.collect_from_expression(action.precondition());
    
    for (const auto& [fluent, polarity] : collector.get_boolean_fluents()) {
        if (polarity == FluentPolarityCollector::Polarity::POSITIVE) {
            analysis.positive_boolean_preconditions.insert(fluent);
        } else {
            analysis.negative_boolean_preconditions.insert(fluent);
        }
    }
    // Store numeric preconditions
    analysis.numeric_preconditions = collector.get_numeric_fluents();
}

void InterferenceAnalyzer::analyze_effects(const Action& action, ActionAnalysis& analysis) const {
    for (const Effect& effect : action.effects()) {
        const Expression& fluent = effect.fluent();
        analysis.all_effects.insert(fluent);

        if (effect.is_conditional()) {
            FluentPolarityCollector collector;
            collector.collect_from_expression(effect.condition());
            for (const auto& fluent_in_cond : collector.get_boolean_fluents()) {
                analysis.conditional_effect_fluents.insert(fluent_in_cond.first);
            }
            for (const auto& fluent_in_cond : collector.get_numeric_fluents()) {
                analysis.conditional_effect_fluents.insert(fluent_in_cond);
            }
        }

        // If we have a Boolean assignment ... 
        if (effect.kind() == EffectExpression::Kind::ASSIGN && fluent.type() && fluent.type()->is_bool()) {
            const Expression& value = effect.value();
            if (value.is_atom() && value.value().is_boolean()) {
                if (value.value().boolean()) { // If we're assigning true
                    analysis.positive_boolean_effects.insert(fluent);
                } else {
                    analysis.negative_boolean_effects.insert(fluent);
                }
            }
        } else {
            // This handles all numeric effects: ASSIGN, INCREASE, and DECREASE
            analysis.numeric_effects.insert(fluent);
            
            // Collect dependencies from the RHS of the numeric effect
            FluentPolarityCollector collector;
            collector.collect_from_expression(effect.value());
            for (const auto& dep : collector.get_numeric_fluents()) {
                analysis.numeric_effect_dependencies.insert(dep);
            }
        }
    }
}


void InterferenceAnalyzer::print_action_analysis() const {
    if (action_analysis_.empty()) {
        std::cout << "No action analysis available" << std::endl;
        return;
    }
    
    std::cout << "\n=== ACTION ANALYSIS RESULTS ===" << std::endl;
    std::cout << "Total actions analyzed: " << action_analysis_.size() << std::endl;
    
    for (const auto& [action, analysis] : action_analysis_) {
        std::cout << "\nAction: " << action.name() << std::endl;
        
        // Print positive boolean preconditions
        if (!analysis.positive_boolean_preconditions.empty()) {
            std::cout << "  Positive boolean preconditions:" << std::endl;
            for (const auto& fluent : analysis.positive_boolean_preconditions) {
                std::cout << "    " << fluent.to_string() << std::endl;
            }
        }
        
        // Print negative boolean preconditions
        if (!analysis.negative_boolean_preconditions.empty()) {
            std::cout << "  Negative boolean preconditions:" << std::endl;
            for (const auto& fluent : analysis.negative_boolean_preconditions) {
                std::cout << "    " << fluent.to_string() << std::endl;
            }
        }
        
        // Print numeric preconditions
        if (!analysis.numeric_preconditions.empty()) {
            std::cout << "  Numeric preconditions:" << std::endl;
            for (const auto& fluent : analysis.numeric_preconditions) {
                std::cout << "    " << fluent.to_string() << std::endl;
            }
        }
        
        // Print boolean effects (true)
        if (!analysis.positive_boolean_effects.empty()) {
            std::cout << "  Boolean effects (make true):" << std::endl;
            for (const auto& fluent : analysis.positive_boolean_effects) {
                std::cout << "    " << fluent.to_string() << std::endl;
            }
        }
        
        // Print boolean effects (false)
        if (!analysis.negative_boolean_effects.empty()) {
            std::cout << "  Boolean effects (make false):" << std::endl;
            for (const auto& fluent : analysis.negative_boolean_effects) {
                std::cout << "    " << fluent.to_string() << std::endl;
            }
        }
        
        // Print numeric effects
        if (!analysis.numeric_effects.empty()) {
            std::cout << "  Numeric effects:" << std::endl;
            for (const auto& fluent : analysis.numeric_effects) {
                std::cout << "    " << fluent.to_string() << std::endl;
            }
        }
        
        // Summary
        int total_preconditions = analysis.positive_boolean_preconditions.size() + 
                                 analysis.negative_boolean_preconditions.size() + 
                                 analysis.numeric_preconditions.size();
        int total_effects = analysis.positive_boolean_effects.size() + 
                           analysis.negative_boolean_effects.size() + 
                           analysis.numeric_effects.size();
        std::cout << "  Summary: " << total_preconditions << " preconditions, " << total_effects << " effects" << std::endl;
    }
    
    std::cout << "\n=== END ACTION ANALYSIS ===" << std::endl;
}

std::vector<const Action*> InterferenceAnalyzer::topological_sort_actions(const std::vector<const Action*>& actions) const {
    std::vector<const Action*> result;
    
    if (actions.size() <= 1) {
        result = actions; // No sorting needed
    } else {
        // Convert actions to node IDs
        std::vector<int> node_ids;
        
        for (const Action* action : actions) {
            int node_id = action->id();
            if (node_id >= 0) { // Valid node ID
                node_ids.push_back(node_id);
            }
        }
        
        if (node_ids.empty()) {
            result = actions; // No valid node IDs found
        } else {
            // Use the graph's topological sort
            std::vector<int> sorted_node_ids = interference_graph_.topological_sort(node_ids);
            
            // Convert back to actions using direct problem access
            for (int node_id : sorted_node_ids) {
                if (node_id >= 0 && static_cast<size_t>(node_id) < problem_->action_count()) {
                    result.push_back(&problem_->action(node_id));
                }
            }
        }
    }
    
    return result;
}

void InterferenceAnalyzer::output_interference_graph_dot(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing" << std::endl;
        return;
    }
    
    std::cout << "Writing interference graph to " << filename << std::endl;
    
    file << "digraph InterferenceGraph {" << std::endl;
    file << "    rankdir=LR;" << std::endl;
    file << "    node [shape=box, style=rounded];" << std::endl;
    file << "    edge [color=red, arrowhead=vee];" << std::endl;
    file << std::endl;
    
    // Write nodes (actions)
    for (size_t i = 0; i < problem_->action_count(); ++i) {
        const Action* action = &problem_->action(i);
        if (action) {
            file << "    " << i << " [label=\"" << action->name() << "\"];" << std::endl;
        }
    }
    
    file << std::endl;
    
    // Write edges (interferences)
    for (int node_id = 0; node_id < static_cast<int>(problem_->action_count()); ++node_id) {
        const auto& neighbors = interference_graph_.get_neighbours(node_id);
        for (int neighbor : neighbors) {
            file << "    " << node_id << " -> " << neighbor << ";" << std::endl;
        }
    }
    
    file << "}" << std::endl;
    file.close();
    
    std::cout << "Interference graph successfully written to " << filename << std::endl;
}

} // namespace planmt