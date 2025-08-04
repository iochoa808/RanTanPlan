#pragma once

#include "interference_analysis.h"
#include <utility>

namespace planmt {

/**
 * @brief Lazy interference analysis implementation
 * 
 * Computes action interferences on-demand and caches results. Provides memory-efficient
 * analysis suitable for use with exists and lazy forall propagators that only need
 * edge-based interference checking.
 * 
 * WARNING: This implementation does NOT support graph-based methods like
 * get_interference_graph() or get_neighbours(). These methods will throw exceptions.
 */
class LazyInterferenceAnalysis : public InterferenceAnalysis {
public:
    /**
     * @brief Construct and initialize the analyzer with a planning problem
     * @param problem The planning problem to analyze
     */
    explicit LazyInterferenceAnalysis(const Problem& problem);
    
    // Default constructor for cases where problem isn't available at construction time
    LazyInterferenceAnalysis() = default;
    
    void initialize(const Problem& problem) override;
    
    // Direct interference checking methods (fully supported)
    bool has_interference(const Action& a1, const Action& a2) const override;
    Graph::NodeId get_action_node_id(const Action& action) const override;
    const Action* get_action_from_node_id(Graph::NodeId node_id) const override;
    std::vector<const Action*> topological_sort_actions(const std::vector<const Action*>& actions) const override;
    
    // Graph-based methods (NOT supported - will throw exceptions)
    const Graph& get_interference_graph() const override;
    const std::vector<Graph::NodeId>& get_neighbours(Graph::NodeId node_id) const override;
    
    // Utility methods
    bool is_lazy() const override { return true; }
    void output_interference_graph_dot(const std::string& filename = "interference.dot") const override;

private:
    
    // Cache for interference results to avoid recomputation
    // Uses string key based on action names for simplicity
    mutable std::unordered_map<std::string, bool> interference_cache_;
    
    /**
     * @brief Check if two actions interfere with each other (internal implementation with caching)
     * @param a1 First action
     * @param a2 Second action
     * @return True if actions interfere, false otherwise
     */
    bool compute_interference(const Action& a1, const Action& a2) const;
    
    
};

} // namespace planmt