#include "interference_analyzer.h"
#include <iostream>
#include <algorithm>

namespace planmt {

void InterferenceAnalyzer::initialize(const Problem& problem) {
    problem_ = &problem;
    
    // Clear any existing data
    action_to_node_id_.clear();
    node_id_to_action_.clear();
    interference_graph_ = Graph();
    
    // Create nodes for each action
    for (const Action& action : problem.actions()) {
        Graph::NodeId node_id = interference_graph_.add_node();
        action_to_node_id_[action] = node_id;
        node_id_to_action_.push_back(&action);
    }
    
    std::cout << "InterferenceAnalyzer initialized with " << problem.actions().size() 
              << " actions" << std::endl;
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

} // namespace planmt
