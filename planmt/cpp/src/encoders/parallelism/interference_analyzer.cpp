#include "interference_analyzer.h"
#include <iostream>
#include <algorithm>

namespace planmt {

InterferenceAnalyzer::InterferenceAnalyzer(const Problem& problem) {
    initialize(problem);
}

void InterferenceAnalyzer::initialize(const Problem& problem) {
    problem_ = &problem;
    
    // Clear any existing data
    action_to_node_id_.clear();
    node_id_to_action_.clear();
    action_analysis_.clear();
    interference_graph_ = Graph();
    
    // Create nodes for each action and analyze them
    for (const Action& action : problem.actions()) {
        Graph::NodeId node_id = interference_graph_.add_node();
        action_to_node_id_[action] = node_id;
        node_id_to_action_.push_back(&action);
        
        // Analyze this action and store the results
        action_analysis_[action] = analyze_action(action);
    }
    
    std::cout << "InterferenceAnalyzer initialized with " << problem.actions().size() 
              << " actions and indexed their preconditions and effects" << std::endl;
    
    // Print the action analysis results
    //print_action_analysis();
    build_interference_graph();
}

void InterferenceAnalyzer::build_interference_graph() {
    if (!problem_) {
        std::cout << "Error: InterferenceAnalyzer not initialized with a problem" << std::endl;
        return;
    }
    
    std::cout << "Building interference graph..." << std::endl;
    analyze_action_conflicts();
    std::cout << "Interference graph built with " << interference_graph_.num_nodes() 
              << " nodes and " << interference_graph_.num_edges() << " edges" << std::endl;
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
                    Graph::NodeId node1 = action_to_node_id_[action1];
                    Graph::NodeId node2 = action_to_node_id_[action2];
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
    
    // a1's positive effects vs a2's negative preconditions
    if (has_intersection(analysis_a1.positive_boolean_effects, 
                        analysis_a2.negative_boolean_preconditions)) {
        return true;
    }
    
    // a1's negative effects vs a2's positive preconditions  
    if (has_intersection(analysis_a1.negative_boolean_effects,
                        analysis_a2.positive_boolean_preconditions)) {
        return true;
    }
    
    // a1's numeric effects vs a2's numeric preconditions
    if (has_intersection(analysis_a1.numeric_effects,
                        analysis_a2.numeric_preconditions)) {
        return true;
    }
    
    // 2. Check if effects of a1 and a2 interfere
    
    // a1's positive effects vs a2's negative effects (same fluent made true/false)
    if (has_intersection(analysis_a1.positive_boolean_effects,
                        analysis_a2.negative_boolean_effects)) {
        return true;
    }
    
    // a1's negative effects vs a2's positive effects (same fluent made false/true)
    if (has_intersection(analysis_a1.negative_boolean_effects,
                        analysis_a2.positive_boolean_effects)) {
        return true;
    }
    
    // numeric effects interfere if they modify the same fluent
    if (has_intersection(analysis_a1.numeric_effects,
                        analysis_a2.numeric_effects)) {
        return true;
    }
    
    return false;
}

bool InterferenceAnalyzer::has_interference(const Action& a1, const Action& a2) const {

    // Retrieve graph node IDs for both actions
    auto it1 = action_to_node_id_.find(a1);
    auto it2 = action_to_node_id_.find(a2);
    if (it1 == action_to_node_id_.end() || it2 == action_to_node_id_.end()) {
        return false;
    }
    // Fast O(1) lookup in the pre-built interference graph
    return interference_graph_.has_edge(it1->second, it2->second);
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
        
        if (effect.kind() == EffectExpression::Kind::ASSIGN) {
            // For assignment effects, check if it's a boolean assignment
            if (fluent.type() && fluent.type()->is_bool()) {
                // Boolean assignment - check if setting to true or false
                const Expression& value = effect.value();
                if (value.is_atom()) {
                    const Atom& atom = value.value();
                    if (atom.is_boolean()) {
                        if (atom.boolean()) {
                            analysis.positive_boolean_effects.insert(fluent);
                        } else {
                            analysis.negative_boolean_effects.insert(fluent);
                        }
                    }
                }
            } else {
                // Numeric assignment
                analysis.numeric_effects.insert(fluent);
            }
        } else if (effect.kind() == EffectExpression::Kind::INCREASE || 
                   effect.kind() == EffectExpression::Kind::DECREASE) {
            // Numeric increase/decrease effects
            analysis.numeric_effects.insert(fluent);
        }
    }
}

Graph::NodeId InterferenceAnalyzer::get_action_node_id(const Action& action) const {
    auto it = action_to_node_id_.find(action);
    return (it != action_to_node_id_.end()) ? it->second : -1;
}

const Action* InterferenceAnalyzer::get_action_from_node_id(Graph::NodeId node_id) const {
    if (node_id >= 0 && static_cast<size_t>(node_id) < node_id_to_action_.size()) {
        return node_id_to_action_[node_id];
    }
    return nullptr;
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

} // namespace planmt
