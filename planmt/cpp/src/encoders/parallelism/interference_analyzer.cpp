#include "interference_analyzer.h"
#include <iostream>
#include <algorithm>

namespace planmt {

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
              << " actions and analyzed their preconditions and effects" << std::endl;
    
    // Print the action analysis results
    print_action_analysis();
}

void InterferenceAnalyzer::build_interference_graph() {
    if (!problem_) {
        std::cout << "Error: InterferenceAnalyzer not initialized with a problem" << std::endl;
        return;
    }
    
    std::cout << "Building interference graph..." << std::endl;
    analyze_action_conflicts();
    std::cout << "Interference graph built with " << interference_graph_.num_nodes() 
              << " nodes" << std::endl;
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
    // TODO: Implement expensive interference analysis (done during preprocessing)
    // This checks if action a1 interferes with action a2 (directional relationship)
    // Examples of interference:
    // - a1's effects conflict with a2's preconditions
    // - a1's effects conflict with a2's effects on the same fluent
    // Note: interference is directional, so a1->a2 doesn't imply a2->a1
    // This method is called O(n²) times during graph building but results are cached
    // For now, return false (no conflicts detected)
    return false;
}

bool InterferenceAnalyzer::has_interference(const Action& a1, const Action& a2) const {
    // Fast O(1) lookup in the pre-built interference graph
    // This is called frequently during encoding, so it needs to be cheap
    auto it1 = action_to_node_id_.find(a1);
    auto it2 = action_to_node_id_.find(a2);
    
    if (it1 == action_to_node_id_.end() || it2 == action_to_node_id_.end()) {
        return false;
    }
    
    return interference_graph_.has_edge(it1->second, it2->second);
}

InterferenceAnalyzer::ActionAnalysis InterferenceAnalyzer::analyze_action(const Action& action) const {
    ActionAnalysis analysis;
    
    // Analyze preconditions
    analyze_preconditions(action, analysis);
    
    // Analyze effects
    analyze_effects(action, analysis);
    
    return analysis;
}

void InterferenceAnalyzer::analyze_preconditions(const Action& action, ActionAnalysis& analysis) const {
    if (!action.has_precondition()) {
        return;
    }
    
    // Use the fluent polarity collector to analyze preconditions
    FluentPolarityCollector collector;
    collector.collect_from_expression(action.precondition());
    
    // Store the results
    analysis.precondition_boolean_fluents = collector.get_boolean_fluents();
    analysis.precondition_numeric_fluents = collector.get_numeric_fluents();
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
        
        // Print boolean preconditions
        if (!analysis.precondition_boolean_fluents.empty()) {
            std::cout << "  Boolean preconditions:" << std::endl;
            for (const auto& [fluent, polarity] : analysis.precondition_boolean_fluents) {
                std::cout << "    " << fluent.to_string() << " [" 
                         << (polarity == FluentPolarityCollector::Polarity::POSITIVE ? "POSITIVE" : "NEGATIVE") 
                         << "]" << std::endl;
            }
        }
        
        // Print numeric preconditions
        if (!analysis.precondition_numeric_fluents.empty()) {
            std::cout << "  Numeric preconditions:" << std::endl;
            for (const auto& fluent : analysis.precondition_numeric_fluents) {
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
        int total_preconditions = analysis.precondition_boolean_fluents.size() + analysis.precondition_numeric_fluents.size();
        int total_effects = analysis.positive_boolean_effects.size() + analysis.negative_boolean_effects.size() + analysis.numeric_effects.size();
        std::cout << "  Summary: " << total_preconditions << " preconditions, " << total_effects << " effects" << std::endl;
    }
    
    std::cout << "\n=== END ACTION ANALYSIS ===" << std::endl;
}

} // namespace planmt
