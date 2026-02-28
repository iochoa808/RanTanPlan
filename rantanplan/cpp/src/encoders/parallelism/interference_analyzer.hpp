#pragma once

#include "../../problem/problem.hpp"
#include "../../problem/action.hpp"
#include "../../problem/visitors/fluent_polarity_collector.hpp"
#include "graph.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rantanplan {

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
    /**
     * @brief Construct and initialize the analyzer with a planning problem
     * @param problem The planning problem to analyze
     */
    explicit InterferenceAnalyzer(const Problem& problem);
    
    // Default constructor for cases where problem isn't available at construction time
    InterferenceAnalyzer() = default;
    
    /**
     * @brief Initialize the analyzer with a planning problem (for use with default constructor)
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
     * @brief Topologically sort a set of actions based on the interference graph
     * @param actions The actions to sort
     * @return Actions sorted in topological order based on interference dependencies
     */
    std::vector<const Action*> topological_sort_actions(const std::vector<const Action*>& actions) const;

    /**
     * @brief Output the interference graph to a DOT format file
     * @param filename The name of the output file (default: "interference.dot")
     */
    void output_interference_graph_dot(const std::string& filename = "interference.dot") const;

private:
    // Data structures to store action analysis results
    struct ActionAnalysis {
        // Precondition analysis
        std::unordered_set<ExprID> positive_boolean_preconditions;  // Boolean fluents required to be true
        std::unordered_set<ExprID> negative_boolean_preconditions;  // Boolean fluents required to be false
        std::unordered_set<ExprID> numeric_preconditions;           // Numeric fluents in preconditions

        // Effect analysis
        std::unordered_set<ExprID> positive_boolean_effects;  // Boolean fluents made true
        std::unordered_set<ExprID> negative_boolean_effects;  // Boolean fluents made false
        std::unordered_set<ExprID> numeric_effects;           // Numeric fluents modified

        // A set of all fluents modified by this action's effects (the union of all the previous 3 sets)
        std::unordered_set<ExprID> all_effects;

        // Fluents appearing in the conditions of conditional effects
        std::unordered_set<ExprID> conditional_effect_fluents;

        // Fluents appearing on the RHS of numeric assignments.
        std::unordered_set<ExprID> numeric_effect_dependencies;
    };
    
    const Problem* problem_;
    Graph interference_graph_;
    
    
    // Analysis results for each action
    std::unordered_map<Action, ActionAnalysis> action_analysis_;
    
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
    
    /**
     * @brief Analyze a single action and populate its ActionAnalysis
     * @param action The action to analyze
     * @return ActionAnalysis structure with collected information
     */
    ActionAnalysis analyze_action(const Action& action) const;
    
    /**
     * @brief Analyze the preconditions of an action
     * @param action The action to analyze
     * @param analysis The analysis structure to populate
     */
    void analyze_preconditions(const Action& action, ActionAnalysis& analysis) const;
    
    /**
     * @brief Analyze the effects of an action
     * @param action The action to analyze
     * @param analysis The analysis structure to populate
     */
    void analyze_effects(const Action& action, ActionAnalysis& analysis) const;

    /**
     * @brief Print the action analysis results for debugging
     */
    void print_action_analysis() const;
};

} // namespace rantanplan
