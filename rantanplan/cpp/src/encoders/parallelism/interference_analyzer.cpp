#include "interference_analyzer.hpp"
#include "../../util/memory_tracker.hpp"
#include "../../util/scoped_timer.hpp"
#include "../../util/logger.hpp"
#include "../../util/stats.hpp"
#include "../../config/config.hpp"
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <fstream>

namespace rantanplan {

InterferenceAnalyzer::InterferenceAnalyzer(const Problem& problem) {
    initialize(problem);
}

void InterferenceAnalyzer::initialize(const Problem& problem) {
    problem_ = &problem;

    // Clear any existing data
    action_analysis_.clear();
    interference_graph_ = Graph();

    // Create nodes for each action and analyze them
    for (const Action& action : problem.actions()) {
        interference_graph_.add_node();

        // Analyze this action and store the results
        action_analysis_[action] = analyze_action(action);
    }

    // Report action analysis completion
    double current_memory = MemoryTracker::instance().get_current_memory_mb();
    Logger::instance().verbose("InterferenceAnalyzer initialized with " + std::to_string(problem.actions().size()) +
                              " actions (memory: " + std::to_string(static_cast<int>(current_memory)) + "MB)");

    build_interference_graph();
}

void InterferenceAnalyzer::build_interference_graph() {
    if (!problem_) {
        Logger::instance().error("InterferenceAnalyzer not initialized with a problem");
        return;
    }

    ScopedTimer timer("interference.graph_build_time_ms");
    double start_memory = MemoryTracker::instance().get_current_memory_mb();

    analyze_action_conflicts();

    // Record stats
    double memory_used = MemoryTracker::instance().get_current_memory_mb() - start_memory;
    Stats::instance().set("interference.nodes", interference_graph_.num_nodes());
    Stats::instance().set("interference.edges", interference_graph_.num_edges());
    Stats::instance().set("interference.memory_mb", memory_used);

    // Structured visual output
    Logger::instance().component(VerbosityLevel::INFO, "Interference", {
        {"time", std::to_string(static_cast<int>(timer.elapsed_ms())) + "ms"},
        {"nodes", std::to_string(interference_graph_.num_nodes())},
        {"edges", std::to_string(interference_graph_.num_edges())},
        {"mem", std::to_string(static_cast<int>(memory_used)) + "MB"}
    });
}

void InterferenceAnalyzer::analyze_action_conflicts() {
    const auto& actions = problem_->actions();

    // Expensive O(n²) preprocessing: analyze all pairs of actions for conflicts
    // Results are cached in the interference graph for fast lookup during execution
    for (size_t i = 0; i < actions.size(); ++i) {
        for (size_t j = 0; j < actions.size(); ++j) {
            if (i != j) {
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

bool InterferenceAnalyzer::actions_interfere(const Action& a1, const Action& a2) const {
     // Get analysis for both actions
    auto it1 = action_analysis_.find(a1);
    auto it2 = action_analysis_.find(a2);

    if (it1 == action_analysis_.end() || it2 == action_analysis_.end()) {
        return false;
    }

    const ActionAnalysis& analysis_a1 = it1->second;
    const ActionAnalysis& analysis_a2 = it2->second;

    auto has_intersection = [](const std::unordered_set<ExprID>& set1,
                              const std::unordered_set<ExprID>& set2) -> bool {
        for (const auto& elem : set1) {
            if (set2.find(elem) != set2.end()) {
                return true;
            }
        }
        return false;
    };

    // 1. Check if effects of a1 can prevent execution of a2
    // Effects of a1 interfere with preconditions of a2
    if (has_intersection(analysis_a1.positive_boolean_effects, analysis_a2.negative_boolean_preconditions) ||
        has_intersection(analysis_a1.negative_boolean_effects, analysis_a2.positive_boolean_preconditions) ||
        has_intersection(analysis_a1.numeric_effects, analysis_a2.numeric_preconditions)) {
        return true;
    }

    // 2. Check if effects of a1 can affect the conditions of a2's conditional effects
    if (has_intersection(analysis_a1.all_effects, analysis_a2.conditional_effect_fluents)) {
        return true;
    }

    // 3. Check if effects of a1 can affect the RHS of a2's numeric effects
    if (has_intersection(analysis_a1.all_effects, analysis_a2.numeric_effect_dependencies)) {
        return true;
    }

    // 4. Check if effects of a1 and a2 interfere (same fluents modified differently or both modified)
    if (has_intersection(analysis_a1.positive_boolean_effects, analysis_a2.negative_boolean_effects) ||
        has_intersection(analysis_a1.negative_boolean_effects, analysis_a2.positive_boolean_effects) ||
        has_intersection(analysis_a1.numeric_effects, analysis_a2.numeric_effects)) {
        return true;
    }

    return false;
}

bool InterferenceAnalyzer::has_interference(const Action& a1, const Action& a2) const {
    return interference_graph_.has_edge(a1.id(), a2.id());
}

InterferenceAnalyzer::ActionAnalysis InterferenceAnalyzer::analyze_action(const Action& action) const {
    ActionAnalysis analysis;
    analyze_preconditions(action, analysis);
    analyze_effects(action, analysis);
    return analysis;
}

void InterferenceAnalyzer::analyze_preconditions(const Action& action, ActionAnalysis& analysis) const {
    if (!action.has_precondition()) {
        return;
    }

    FluentPolarityCollector collector(*problem_);
    collector.collect_from_id(action.precondition_id());

    for (const auto& [eid, polarity] : collector.get_boolean_fluents()) {
        if (polarity == FluentPolarityCollector::Polarity::POSITIVE) {
            analysis.positive_boolean_preconditions.insert(eid);
        } else {
            analysis.negative_boolean_preconditions.insert(eid);
        }
    }
    for (ExprID eid : collector.get_numeric_fluents()) {
        analysis.numeric_preconditions.insert(eid);
    }
}

void InterferenceAnalyzer::analyze_effects(const Action& action, ActionAnalysis& analysis) const {
    for (const Effect& effect : action.effects()) {
        const EffectExpression& eff_expr = effect.effect_expression();
        ExprID fluent_eid = eff_expr.fluent_id();
        analysis.all_effects.insert(fluent_eid);

        if (effect.is_conditional()) {
            FluentPolarityCollector collector(*problem_);
            collector.collect_from_id(effect.effect_expression().condition_id());
            for (const auto& [eid, _] : collector.get_boolean_fluents()) {
                analysis.conditional_effect_fluents.insert(eid);
            }
            for (ExprID eid : collector.get_numeric_fluents()) {
                analysis.conditional_effect_fluents.insert(eid);
            }
        }

        if (effect.kind() == EffectExpression::Kind::ASSIGN && problem_->is_bool_type(fluent_eid)) {
            ExprID value_eid = eff_expr.value_id();
            const ExprPool& pool = problem_->pool();
            if (pool.is_constant(value_eid) && pool.payload_is_bool(value_eid)) {
                if (pool.payload_bool(value_eid)) {
                    analysis.positive_boolean_effects.insert(fluent_eid);
                } else {
                    analysis.negative_boolean_effects.insert(fluent_eid);
                }
            }
        } else {
            // This handles all numeric effects
            analysis.numeric_effects.insert(fluent_eid);

            // Collect dependencies from the RHS of the numeric effect
            FluentPolarityCollector collector(*problem_);
            collector.collect_from_id(effect.effect_expression().value_id());
            for (ExprID dep : collector.get_numeric_fluents()) {
                analysis.numeric_effect_dependencies.insert(dep);
            }
        }
    }
}


void InterferenceAnalyzer::print_action_analysis() const {
    if (action_analysis_.empty()) {
        std::cout << "No action analysis available" << std::endl;
        return;
    }

    std::cout << "\n=== ACTION ANALYSIS RESULTS ===" << std::endl;
    std::cout << "Total actions analyzed: " << action_analysis_.size() << std::endl;

    for (const auto& [action, analysis] : action_analysis_) {
        std::cout << "\nAction: " << action.name() << std::endl;

        if (!analysis.positive_boolean_preconditions.empty()) {
            std::cout << "  Positive boolean preconditions:" << std::endl;
            for (const auto& eid : analysis.positive_boolean_preconditions) {
                std::cout << "    " << problem_->pool().to_string(eid) << std::endl;
            }
        }

        if (!analysis.negative_boolean_preconditions.empty()) {
            std::cout << "  Negative boolean preconditions:" << std::endl;
            for (const auto& eid : analysis.negative_boolean_preconditions) {
                std::cout << "    " << problem_->pool().to_string(eid) << std::endl;
            }
        }

        if (!analysis.numeric_preconditions.empty()) {
            std::cout << "  Numeric preconditions:" << std::endl;
            for (const auto& eid : analysis.numeric_preconditions) {
                std::cout << "    " << problem_->pool().to_string(eid) << std::endl;
            }
        }

        if (!analysis.positive_boolean_effects.empty()) {
            std::cout << "  Boolean effects (make true):" << std::endl;
            for (const auto& eid : analysis.positive_boolean_effects) {
                std::cout << "    " << problem_->pool().to_string(eid) << std::endl;
            }
        }

        if (!analysis.negative_boolean_effects.empty()) {
            std::cout << "  Boolean effects (make false):" << std::endl;
            for (const auto& eid : analysis.negative_boolean_effects) {
                std::cout << "    " << problem_->pool().to_string(eid) << std::endl;
            }
        }

        if (!analysis.numeric_effects.empty()) {
            std::cout << "  Numeric effects:" << std::endl;
            for (const auto& eid : analysis.numeric_effects) {
                std::cout << "    " << problem_->pool().to_string(eid) << std::endl;
            }
        }

        int total_preconditions = analysis.positive_boolean_preconditions.size() +
                                 analysis.negative_boolean_preconditions.size() +
                                 analysis.numeric_preconditions.size();
        int total_effects = analysis.positive_boolean_effects.size() +
                           analysis.negative_boolean_effects.size() +
                           analysis.numeric_effects.size();
        std::cout << "  Summary: " << total_preconditions << " preconditions, " << total_effects << " effects" << std::endl;
    }

    std::cout << "\n=== END ACTION ANALYSIS ===" << std::endl;
}

std::vector<const Action*> InterferenceAnalyzer::topological_sort_actions(const std::vector<const Action*>& actions) const {
    std::vector<const Action*> result;

    if (actions.size() <= 1) {
        result = actions; // No sorting needed
    } else {
        std::vector<int> node_ids;

        for (const Action* action : actions) {
            int node_id = action->id();
            if (node_id >= 0) {
                node_ids.push_back(node_id);
            }
        }

        if (node_ids.empty()) {
            result = actions;
        } else {
            std::vector<int> sorted_node_ids = interference_graph_.topological_sort(node_ids);

            for (int node_id : sorted_node_ids) {
                if (node_id >= 0 && static_cast<size_t>(node_id) < problem_->action_count()) {
                    result.push_back(&problem_->action(node_id));
                }
            }
        }
    }

    return result;
}

void InterferenceAnalyzer::output_interference_graph_dot(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        Logger::instance().error("Could not open file " + filename + " for writing");
        return;
    }

    Logger::instance().info("Writing interference graph to " + filename);

    file << "digraph InterferenceGraph {" << std::endl;
    file << "    rankdir=LR;" << std::endl;
    file << "    node [shape=box, style=rounded];" << std::endl;
    file << "    edge [color=red, arrowhead=vee];" << std::endl;
    file << std::endl;

    for (size_t i = 0; i < problem_->action_count(); ++i) {
        const Action* action = &problem_->action(i);
        if (action) {
            file << "    " << i << " [label=\"" << action->name() << "\"];" << std::endl;
        }
    }

    file << std::endl;

    for (int node_id = 0; node_id < static_cast<int>(problem_->action_count()); ++node_id) {
        const auto& neighbors = interference_graph_.get_neighbours(node_id);
        for (int neighbor : neighbors) {
            file << "    " << node_id << " -> " << neighbor << ";" << std::endl;
        }
    }

    file << "}" << std::endl;
    file.close();

    Logger::instance().info("Interference graph successfully written to " + filename);
}

} // namespace rantanplan
