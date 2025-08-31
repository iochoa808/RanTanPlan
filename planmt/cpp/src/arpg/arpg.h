#pragma once

#include <vector>
#include <memory>
#include <unordered_set>
#include "supporter.h"
#include "relaxed_state.h"
#include "../problem/problem.h"
#include "../problem/action.h"

namespace planmt {

/**
 * @brief Asymptotic Relaxed Planning Graph implementation
 * 
 * Based on the ARPG construction described in the paper (Algorithm 1).
 * Builds alternating interval (fact) and supporter (action) layers until
 * the goal is reached or no new supporters can be applied.
 */
class ARPG {
public:
    // Layer types in the ARPG
    struct IntervalLayer {
        RelaxedState state;
        int layer_number;
        
        IntervalLayer(const RelaxedState& s, int layer) 
            : state(s), layer_number(layer) {}
    };
    
    struct SupporterLayer {
        std::vector<std::shared_ptr<Supporter>> applicable_supporters;
        int layer_number;
        
        SupporterLayer(int layer) : layer_number(layer) {}
    };
    
    // Constructor
    ARPG(const Problem& problem);
    
    // Main construction algorithm (Algorithm 1 from paper)
    bool construct_graph();
    
    // Check if goal is reachable
    bool is_goal_reachable() const;
    
    // Get the final relaxed state
    const RelaxedState& get_final_state() const;
    
    // Get bounds for all state variables from the final relaxed state
    // Returns map from Expression (state variable) to its computed bounds
    std::unordered_map<Expression, Interval> get_state_variable_bounds() const;
    
    // Get construction statistics
    size_t get_num_layers() const { return interval_layers_.size(); }
    size_t get_num_supporters() const { return supporters_.size(); }
    
    // Print the graph construction step by step
    void print_construction_steps() const;
    
    // Export ARPG as DOT file for visualization
    void export_dot_file(const std::string& filename) const;
    
    // String representation of the entire graph
    std::string to_string() const;

private:
    const Problem& problem_;
    std::vector<std::shared_ptr<Supporter>> supporters_;
    std::vector<IntervalLayer> interval_layers_;
    std::vector<SupporterLayer> supporter_layers_;
    
    bool goal_reached_;
    
    
    // Create supporters from actions (Definition 9 from paper)
    void create_supporters_from_actions();
    
    // Create a supporter for an additive numeric effect
    std::vector<std::shared_ptr<Supporter>> create_supporters_for_effect(
        const Action& action, const Effect& effect);
    
    // Check if any new supporters are applicable
    std::vector<std::shared_ptr<Supporter>> find_applicable_supporters(
        const RelaxedState& state, const std::unordered_set<size_t>& used_supporters) const;
    
    // Apply supporters to create next interval layer
    RelaxedState apply_supporters(const RelaxedState& current_state,
                                  const std::vector<std::shared_ptr<Supporter>>& supporters) const;
    
    // Check if goal conditions are satisfied in the relaxed state
    bool check_goal_satisfaction(const RelaxedState& state) const;
    
    // Initialize the first interval layer from initial state
    RelaxedState create_initial_relaxed_state() const;
    
};

} // namespace planmt