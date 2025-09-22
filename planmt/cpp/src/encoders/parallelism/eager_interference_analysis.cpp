#include "eager_interference_analysis.hpp"
#include "../../util/memory_tracker.hpp"
#include "../../config/config.hpp"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <chrono>

namespace planmt {

EagerInterferenceAnalysis::EagerInterferenceAnalysis(const Problem& problem) {
    initialize(problem);
}

void EagerInterferenceAnalysis::initialize(const Problem& problem) {
    problem_ = &problem;
    
    // Clear any existing data
    action_analysis_.clear();
    interference_graph_ = Graph();
    
    // Setup common functionality using base class methods
    analyze_all_actions();
    
    // Create nodes in the interference graph to match action IDs
    for (size_t i = 0; i < problem.actions().size(); ++i) {
        interference_graph_.add_node();
    }

    // Report memory usage after action analysis
    double current_memory = MemoryTracker::instance().get_current_memory_mb();
    std::cout << "EagerInterferenceAnalysis initialized with " << problem.actions().size() 
              << " actions and indexed their preconditions and effects. "
              << "Memory: " << current_memory << " MB" << std::endl;
    
    build_interference_graph();
}

void EagerInterferenceAnalysis::build_interference_graph() {
    if (!problem_) {
        std::cout << "Error: EagerInterferenceAnalysis not initialized with a problem" << std::endl;
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

void EagerInterferenceAnalysis::analyze_action_conflicts() {
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


bool EagerInterferenceAnalysis::has_interference(const Action& a1, const Action& a2) const {
    // DIRECTIONAL CHECK: Does a1 interfere with a2? (a1 -> a2)
    // This is NOT symmetric: has_interference(A,B) != has_interference(B,A) in general
    return interference_graph_.has_edge(a1.id(), a2.id());
}

bool EagerInterferenceAnalysis::has_interference(int node_id1, int node_id2) const {
    // DIRECTIONAL CHECK: Does node_id1 interfere with node_id2? (node_id1 -> node_id2)
    // This is NOT symmetric: has_interference(A,B) != has_interference(B,A) in general
    // This optimized version works directly with node IDs, avoiding Action object lookups
    return interference_graph_.has_edge(node_id1, node_id2);
}


const Graph& EagerInterferenceAnalysis::get_interference_graph() const {
    return interference_graph_;
}

const std::vector<int>& EagerInterferenceAnalysis::get_neighbours(int node_id) const {
    return interference_graph_.get_neighbours(node_id);
}

std::vector<const Action*> EagerInterferenceAnalysis::topological_sort_actions(const std::vector<const Action*>& actions) const {
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


void EagerInterferenceAnalysis::output_interference_graph_dot(const std::string& filename) const {
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
        const Action& action = problem_->action(i);
        file << "    " << i << " [label=\"" << action.name() << "\"];" << std::endl;
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