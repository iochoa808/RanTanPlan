#pragma once

#include "interference_analysis.h"
#include "../../problem/problem.h"
#include <memory>

namespace planmt {

/**
 * @brief Factory for creating interference analysis instances
 * 
 * Creates either eager or lazy interference analysis implementations:
 * 
 * EAGER ANALYSIS:
 * - Pre-computes full interference graph (O(n²) time and space)
 * - Fast O(1) lookups for any interference query
 * - Supports both direct methods (has_interference) and graph methods (get_interference_graph, get_neighbours) 
 * - Required for: forall/exists semantics, forall propagator
 * 
 * LAZY ANALYSIS:
 * - Computes interferences on-demand with caching
 * - Memory efficient, only stores what's actually queried
 * - Only supports direct methods (has_interference, topological_sort_actions)
 * - Graph methods (get_interference_graph, get_neighbours) will throw exceptions
 * - Suitable for: exists propagator, lazy forall propagator
 */
class InterferenceAnalysisFactory {
public:
    /**
     * @brief Create an interference analysis instance
     * 
     * @param problem The planning problem to analyze
     * @param use_lazy If true, creates LazyInterferenceAnalysis; if false, creates EagerInterferenceAnalysis
     * @return Unique pointer to the created interference analysis instance
     */
    static std::unique_ptr<InterferenceAnalysis> create(const Problem& problem, bool use_lazy = false);
    
    /**
     * @brief Create an interference analysis instance using global config
     * 
     * Uses Config::instance().interference_analyzer.lazy_computation to decide between eager/lazy.
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