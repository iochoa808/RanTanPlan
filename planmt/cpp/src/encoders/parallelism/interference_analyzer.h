#pragma once

#include "../../problem/problem.h"
#include "../../problem/action.h"
#include "graph.h"
#include <unordered_map>
#include <vector>

namespace planmt {

/**
 * @brief Analyzes action interferences and builds an interference graph
 * 
 * This class is responsible for analyzing which actions interfere with each other
 * in a planning problem and building a graph representation of these interferences.
 * Different parallelism strategies can then interpret this graph according to their
 * specific semantics.
 */
class InterferenceAnalyzer {
public:
    InterferenceAnalyzer() = default;
    
    /**
     * @brief Initialize the analyzer with a planning problem
     * @param problem The planning problem to analyze
     */
    void initialize(const Problem& problem);
    
    /**
     * @brief Build the interference graph from the problem
     * 
     * Analyzes all actions in the problem and creates edges between
     * actions that interfere with each other.
     */
    void build_interference_graph();
    
    /**
     * @brief Get the built interference graph
     * @return Reference to the interference graph
     */
    const Graph& get_interference_graph() const { return interference_graph_; }
    
    /**
     * @brief Check if two actions interfere with each other
     * @param a1 First action
     * @param a2 Second action
     * @return True if actions interfere, false otherwise
     */
    bool has_interference(const Action& a1, const Action& a2) const;
    
    /**
     * @brief Get the node ID for an action in the interference graph
     * @param action The action to get the node ID for
     * @return Node ID in the graph, or -1 if action not found
     */
    Graph::NodeId get_action_node_id(const Action& action) const;
    
    /**
     * @brief Get the action corresponding to a node ID
     * @param node_id The node ID to get the action for
     * @return Reference to the action, or nullptr if node ID invalid
     */
    const Action* get_action_from_node_id(Graph::NodeId node_id) const;

private:
    const Problem* problem_;
    Graph interference_graph_;
    
    // Bidirectional mapping between actions and graph nodes
    std::unordered_map<Action, Graph::NodeId> action_to_node_id_;
    std::vector<const Action*> node_id_to_action_;
    
    /**
     * @brief Analyze conflicts between all pairs of actions
     */
    void analyze_action_conflicts();
    
    /**
     * @brief Check if two actions interfere with each other
     * @param a1 First action
     * @param a2 Second action
     * @return True if actions interfere, false otherwise
     */
    bool actions_interfere(const Action& a1, const Action& a2) const;
};

} // namespace planmt
