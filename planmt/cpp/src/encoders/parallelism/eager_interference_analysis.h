#pragma once

#include "interference_analysis.h"

namespace planmt {

/**
 * @brief Eager interference analysis implementation
 * 
 * Pre-computes all action interferences and builds a complete interference graph.
 * Provides fast O(1) lookup for interference queries but uses more memory.
 * This is a refactored version of the original InterferenceAnalyzer.
 */
class EagerInterferenceAnalysis : public InterferenceAnalysis {
public:
    /**
     * @brief Construct and initialize the analyzer with a planning problem
     * @param problem The planning problem to analyze
     */
    explicit EagerInterferenceAnalysis(const Problem& problem);
    
    // Default constructor for cases where problem isn't available at construction time
    EagerInterferenceAnalysis() = default;
    
    void initialize(const Problem& problem) override;
    
    /**
     * @brief Checks if action a1 interferes with action a2.
     * 
     * @param a1 The first action to check.
     * @param a2 The second action to check.
     * @return true if a1 interferes with a2, false otherwise.
     */
    bool has_interference(const Action& a1, const Action& a2) const override;
    bool has_interference(int node_id1, int node_id2) const override;
    std::vector<const Action*> topological_sort_actions(const std::vector<const Action*>& actions) const override;
    
    // Graph-based methods (fully supported by eager analysis)
    const Graph& get_interference_graph() const override;
    const std::vector<int>& get_neighbours(int node_id) const override;
    
    // Utility methods
    bool is_lazy() const override { return false; }
    void output_interference_graph_dot(const std::string& filename = "interference.dot") const override;

private:
    Graph interference_graph_;
    
    /**
     * @brief Build the interference graph from the problem
     * 
     * Analyzes all actions in the problem and creates edges between
     * actions that interfere with each other.
     */
    void build_interference_graph();
    
    /**
     * @brief Analyze conflicts between all pairs of actions
     */
    void analyze_action_conflicts();

};

} // namespace planmt