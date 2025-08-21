#pragma once

#include "interference_analysis.h"
#include "../../problem/problem.h"
#include <memory>
#include <string>

namespace planmt {

/**
 * @brief Factory for creating interference analysis instances
 * 
 * Creates interference analysis implementations based on type:
 * 
 * EAGER ANALYSIS ("eager"):
 * - Pre-computes full interference graph (O(n²) time and space)
 * - Fast O(1) lookups for any interference query
 * - Supports both direct methods (has_interference) and graph methods (get_interference_graph, get_neighbours)
 * - Uses syntactic interference checking
 * 
 * LAZY ANALYSIS ("lazy"):
 * - Computes interferences on-demand with caching
 * - Memory efficient, only stores what's actually queried
 * - Only supports direct methods (has_interference, topological_sort_actions)
 * - Uses syntactic interference checking
 * 
 * SEMANTIC ANALYSIS ("semantic"):
 * - Computes interferences on-demand using SMT solver
 * - Uses semantic interference checking via Z3
 * - Only supports direct methods (has_interference, topological_sort_actions)
 * - More precise than syntactic but computationally expensive
 */
class InterferenceAnalysisFactory {
public:
    /**
     * @brief Create an interference analysis instance
     * 
     * @param problem The planning problem to analyze
     * @param analysis_type Type of analysis: "eager", "lazy", or "semantic"
     * @return Unique pointer to the created interference analysis instance
     */
    static std::unique_ptr<InterferenceAnalysis> create(const Problem& problem, const std::string& analysis_type = "eager");
    
    /**
     * @brief Create an interference analysis instance using global config
     * 
     * Uses Config::instance().interference_analyzer.type to determine the analysis strategy.
     * 
     * @param problem The planning problem to analyze
     * @return Unique pointer to the created interference analysis instance
     */
    static std::unique_ptr<InterferenceAnalysis> create_from_config(const Problem& problem);

private:
    // Factory class should not be instantiated
    InterferenceAnalysisFactory() = delete;
    ~InterferenceAnalysisFactory() = delete;
    InterferenceAnalysisFactory(const InterferenceAnalysisFactory&) = delete;
    InterferenceAnalysisFactory& operator=(const InterferenceAnalysisFactory&) = delete;
};

} // namespace planmt