#include "lazy_interference_analysis.hpp"
#include "../../util/memory_tracker.hpp"
#include "../../util/logger.hpp"
#include "../../config/config.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <unordered_set>

namespace rantanplan {

LazyInterferenceAnalysis::LazyInterferenceAnalysis(const Problem& problem) {
    initialize(problem);
}

void LazyInterferenceAnalysis::initialize(const Problem& problem) {
    problem_ = &problem;

    // Clear any existing data
    action_analysis_.clear();
    interference_cache_.clear();

    // Setup common functionality using base class methods
    analyze_all_actions();

    // Report initialization completion (lazy analysis doesn't pre-build graph)
    double current_memory = MemoryTracker::instance().get_current_memory_mb();
    Logger::instance().verbose("LazyInterferenceAnalysis initialized with " + std::to_string(problem.actions().size()) +
                              " actions (memory: " + std::to_string(static_cast<int>(current_memory)) + "MB)");
}

bool LazyInterferenceAnalysis::has_interference(const Action& a1, const Action& a2) const {
    return compute_interference(a1, a2);
}

bool LazyInterferenceAnalysis::has_interference(int node_id1, int node_id2) const {
    // Convert node IDs back to actions, then use existing compute_interference logic
    const Action* action1 = &problem_->action(node_id1);
    const Action* action2 = &problem_->action(node_id2);
    return compute_interference(*action1, *action2);
}

bool LazyInterferenceAnalysis::compute_interference(const Action& a1, const Action& a2) const {
    // Use action IDs as cache key (unique per ground action instance).
    // Names alone are not unique — e.g. C++ grounding produces multiple
    // ground actions all named "pick" with different parameters.
    auto cache_key = std::make_pair(a1.id(), a2.id());
    
    // Check if result is already cached
    auto cache_it = interference_cache_.find(cache_key);
    if (cache_it != interference_cache_.end()) {
        return cache_it->second;
    }
    
    // Compute directional interference: does a1 interfere with a2?
    bool interferes = actions_interfere(a1, a2);
    interference_cache_[cache_key] = interferes;
    
    return interferes;
}





std::vector<const Action*> LazyInterferenceAnalysis::topological_sort_actions(const std::vector<const Action*>& actions) const {
    if (actions.size() <= 1) {
        std::vector<const Action*> result;
        result.reserve(actions.size());
        for (const auto& action : actions) {
            result.push_back(action);
        }
        return result;
    }
    
    // Convert actions to their existing node IDs from the base class mappings
    std::vector<int> node_ids;
    for (const Action* action : actions) {
        int node_id = action->id();
        if (node_id >= 0) { // Valid node ID found
            node_ids.push_back(node_id);
        }
    }
    
    if (node_ids.empty()) {
        return actions; // No valid node IDs found, return as-is
    }
    
    // Build a temporary subgraph using existing node IDs and cached interferences
    // This reuses all existing mappings and cached interference computations
    Graph temp_subgraph(problem_->action_count());
    
    // Add edges using cached interference results (no recomputation needed!)
    for (size_t i = 0; i < actions.size(); ++i) {
        for (size_t j = 0; j < actions.size(); ++j) {
            if (i != j) {
                const Action* action1 = actions[i];
                const Action* action2 = actions[j];
                
                // Use cached interference check - no recomputation since this runs after planning
                if (has_interference(*action1, *action2)) {
                    int node1 = action1->id();
                    int node2 = action2->id();
                    if (node1 >= 0 && node2 >= 0) {
                        temp_subgraph.add_edge(node1, node2);
                    }
                }
            }
        }
    }
    
    // Use Graph's topological sort which implements DFS post-order traversal
    // This produces EXECUTION ORDER (reverse topological order):
    // - If A interferes with B (A -> B), then B appears before A in the result
    // - This ensures safe execution: interfered-with actions execute first
    std::vector<int> sorted_node_ids = temp_subgraph.topological_sort(node_ids);
    
    // Convert back to actions using existing base class mappings
    std::vector<const Action*> result;
    for (int node_id : sorted_node_ids) {
        const Action* action = &problem_->action(node_id);
        if (action) {
            result.push_back(action);
        }
    }
    
    return result;
}

// Graph-based methods (NOT supported by lazy analysis)

const Graph& LazyInterferenceAnalysis::get_interference_graph() const {
    throw std::runtime_error("LazyInterferenceAnalysis does not support get_interference_graph(). "
                            "This method is only available with eager analysis. "
                            "Use has_interference() for individual interference checks instead.");
}

const std::vector<int>& LazyInterferenceAnalysis::get_neighbours(int node_id) const {
    throw std::runtime_error("LazyInterferenceAnalysis does not support get_neighbours(). "
                            "This method is only available with eager analysis. "
                            "Use has_interference() for individual interference checks instead.");
}

void LazyInterferenceAnalysis::output_interference_graph_dot(const std::string& filename) const {
    throw std::runtime_error("LazyInterferenceAnalysis does not support output_interference_graph_dot(). "
                            "This method is only available with eager analysis as it requires the full graph.");
}

} // namespace rantanplan