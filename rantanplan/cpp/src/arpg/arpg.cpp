#include "arpg.hpp"
#include "../config/config.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <limits>

namespace rantanplan {

// Supporter class implementation
Supporter::Supporter(const std::string& name, const std::string& affected_variable,
                     const Action& source_action, const Effect& effect,
                     const Problem& problem)
    : name_(name), affected_variable_(affected_variable),
      source_action_(source_action), effect_(effect), problem_(&problem) {}

bool Supporter::is_applicable(const RelaxedState& state) const {
    // Check if action preconditions are satisfied in the relaxed state
    return state.satisfies_condition(source_action_.precondition_id(), problem_->pool());
}

void Supporter::apply_effect(RelaxedState& state) const {
    // Handle boolean effects (propositions)
    if (problem_->is_bool_type(effect_.effect_expression().fluent_id()) ||
        (!effect_.effect_expression().is_increase() &&
         !effect_.effect_expression().is_decrease() &&
         !effect_.effect_expression().is_assign())) {
        // Boolean add effect (relaxed planning ignores delete effects)
        state.add_proposition(affected_variable_);
        return;
    }

    // Handle numeric effects
    auto interval_opt = state.get_variable(affected_variable_);
    Interval current_interval = interval_opt.value_or(Interval(0.0));
    double current_value = current_interval.midpoint();

    Interval effect_interval = compute_effect_interval(current_value);
    state.extend_variable(affected_variable_, effect_interval);
}

Interval Supporter::compute_effect_interval(double current_value) const {
    if (effect_.effect_expression().is_increase()) {
        // Increase effect: (current_value, +∞)
        return Interval(current_value, std::numeric_limits<double>::infinity());
    } else if (effect_.effect_expression().is_decrease()) {
        // Decrease effect: (-∞, current_value)
        return Interval(-std::numeric_limits<double>::infinity(), current_value);
    } else if (effect_.effect_expression().is_assign()) {
        // Assignment: optimistically can assign any positive value
        return Interval(0.0, std::numeric_limits<double>::infinity());
    } else {
        // Default case
        return Interval(0.0, std::numeric_limits<double>::infinity());
    }
}

std::string Supporter::to_string() const {
    std::string result = "Supporter[" + name_ + "] " + affected_variable_ + " := ";

    if (effect_.effect_expression().is_increase()) {
        result += "(x, +∞) [increase]";
    } else if (effect_.effect_expression().is_decrease()) {
        result += "(-∞, x) [decrease]";
    } else if (effect_.effect_expression().is_assign()) {
        result += "[0, +∞) [assign]";
    } else {
        result += "true [boolean]";
    }

    result += " from " + source_action_.name();
    return result;
}

// ARPG class implementation
ARPG::ARPG(const Problem& problem)
    : problem_(problem), goal_reached_(false), iteration_count_(0) {
    double current_memory = MemoryTracker::instance().get_current_memory_mb();
    Logger::instance().debug("[ARPG] Starting, memory=" + std::to_string(static_cast<int>(current_memory)) + "MB");
    create_supporters();
}

bool ARPG::construct_graph() {
    auto& config = Config::instance();
    ScopedTimer timer("arpg.build_time_ms");
    double start_memory = MemoryTracker::instance().get_current_memory_mb();

    // Initialize with the initial relaxed state
    current_state_ = create_initial_state();
    used_supporters_.assign(supporters_.size(), false);

    const int MAX_ITERATIONS = 100;
    for (iteration_count_ = 0; iteration_count_ < MAX_ITERATIONS; ++iteration_count_) {
        // Store debug info
        if (config.is_info()) {
            DebugIteration debug_iter;
            debug_iter.iteration = iteration_count_;
            debug_iter.state_before = current_state_;
            debug_iter.goal_satisfied = check_goal_satisfaction();
            debug_iterations_.push_back(debug_iter);
        }

        // Check if goal is satisfied
        if (check_goal_satisfaction()) {
            goal_reached_ = true;
            Logger::instance().debug("[ARPG] reached goal at iteration " + std::to_string(iteration_count_));
            break;
        }

        // Find applicable supporters that haven't been used
        auto applicable = find_applicable_supporters();

        // If no new supporters, terminate
        if (applicable.empty()) {
            Logger::instance().debug("[ARPG] No more applicable supporters at iteration " + std::to_string(iteration_count_));
            break;
        }

        // Store applied supporters for debug info
        if (config.is_info()) {
            debug_iterations_.back().applied_supporters = applicable;
        }

        // Apply supporters
        apply_supporters(applicable);

        // Mark supporters as used
        for (size_t idx : applicable) {
            used_supporters_[idx] = true;
        }
    }

    double end_memory = MemoryTracker::instance().get_current_memory_mb();
    double memory_used = end_memory - start_memory;

    // Record to Stats
    Stats::instance().set("arpg.iterations", iteration_count_);
    Stats::instance().set("arpg.supporters_created", supporters_.size());
    Stats::instance().set("arpg.memory_mb", memory_used);
    Stats::instance().set("arpg.goal_reached", goal_reached_ ? 1.0 : 0.0);

    // Structured visual output
    Logger::instance().component(VerbosityLevel::INFO, "ARPG", {
        {"time", std::to_string(static_cast<int>(timer.elapsed_ms())) + "ms"},
        {"iterations", std::to_string(iteration_count_)},
        {"supporters", std::to_string(supporters_.size())},
        {"mem", std::to_string(static_cast<int>(memory_used)) + "MB"}
    });

    return goal_reached_;
}

void ARPG::create_supporters() {
    supporters_.clear();

    const ExprPool& pool = problem_.pool();
    for (const auto& action : problem_.actions()) {
        for (const auto& effect : action.effects()) {
            std::string var_name = pool.to_string(effect.effect_expression().fluent_id());
            if (!var_name.empty()) {
                std::string supporter_name = action.name() + "_" + var_name;
                supporters_.emplace_back(supporter_name, var_name, action, effect, problem_);
            }
        }
    }

    Logger::instance().debug("[ARPG] Created " + std::to_string(supporters_.size()) +
                            " supporters from " + std::to_string(problem_.actions().size()) + " actions");
}

std::vector<size_t> ARPG::find_applicable_supporters() const {
    std::vector<size_t> applicable;

    for (size_t i = 0; i < supporters_.size(); ++i) {
        if (!used_supporters_[i] && supporters_[i].is_applicable(current_state_)) {
            applicable.push_back(i);
        }
    }

    return applicable;
}

void ARPG::apply_supporters(const std::vector<size_t>& indices) {
    for (size_t idx : indices) {
        supporters_[idx].apply_effect(current_state_);
    }
}

bool ARPG::check_goal_satisfaction() const {
    const ExprPool& pool = problem_.pool();
    for (const auto& goal_condition : problem_.goals()) {
        if (!current_state_.satisfies_condition(goal_condition.goal_id(), pool)) {
            return false;
        }
    }
    return true;
}

RelaxedState ARPG::create_initial_state() const {
    RelaxedState initial_state;
    const ExprPool& pool = problem_.pool();

    // Process all initial state assignments
    for (const auto& assignment : problem_.initial_state()) {
        std::string var_name = pool.to_string(assignment.fluent_id());

        // Handle numeric assignments
        ExprID val_eid = assignment.value_id();
        if (pool.is_constant(val_eid)) {
            if (pool.payload_is_bool(val_eid)) {
                if (pool.payload_bool(val_eid)) {
                    initial_state.add_proposition(var_name);
                }
            } else if (pool.payload_is_double(val_eid)) {
                double value = pool.payload_double(val_eid);
                initial_state.set_variable(var_name, Interval(value));
            } else if (pool.payload_is_int(val_eid)) {
                double value = static_cast<double>(pool.payload_int(val_eid));
                initial_state.set_variable(var_name, Interval(value));
            }
        } else {
            // For non-constant assignments, treat as propositions
            initial_state.add_proposition(var_name);
        }
    }

    return initial_state;
}

std::unordered_map<ExprID, Interval> ARPG::get_state_variable_bounds() const {
    std::unordered_map<ExprID, Interval> bounds_map;

    for (ExprID eid : problem_.grounded_fluents()) {
        std::string var_name = problem_.pool().to_string(eid);
        auto interval_opt = current_state_.get_variable(var_name);
        if (interval_opt.has_value()) {
            bounds_map[eid] = interval_opt.value();
        }
    }

    return bounds_map;
}

std::vector<ARPG::SupporterOrderingInfo> ARPG::get_supporter_ordering() const {
    std::vector<SupporterOrderingInfo> ordering;

    for (const auto& debug_iter : debug_iterations_) {
        for (size_t supporter_idx : debug_iter.applied_supporters) {
            const Supporter& supporter = supporters_[supporter_idx];
            const Action& action = supporter.source_action();
            ordering.emplace_back(supporter, debug_iter.iteration, action);
        }
    }

    return ordering;
}

std::vector<const Action*> ARPG::get_action_ordering() const {
    std::vector<const Action*> action_ordering;

    std::unordered_set<const Action*> seen_actions;

    for (const auto& debug_iter : debug_iterations_) {
        for (size_t supporter_idx : debug_iter.applied_supporters) {
            const Action* action = &supporters_[supporter_idx].source_action();

            // Add action only on its first appearance
            if (seen_actions.find(action) == seen_actions.end()) {
                action_ordering.push_back(action);
                seen_actions.insert(action);
            }
        }
    }

    // Add any remaining actions that didn't appear in ARPG
    for (const Action& action : problem_.actions()) {
        if (seen_actions.find(&action) == seen_actions.end()) {
            action_ordering.push_back(&action);
        }
    }

    return action_ordering;
}

void ARPG::print_construction_steps() const {
    std::cout << "\n=== ARPG Construction Steps ===" << std::endl;
    std::cout << "Total supporters created: " << supporters_.size() << std::endl;

    for (const auto& supporter : supporters_) {
        std::cout << "  " << supporter.to_string() << std::endl;
    }

    std::cout << "\n=== Iteration-by-Iteration Construction ===" << std::endl;

    for (const auto& debug_iter : debug_iterations_) {
        std::cout << "\nIteration " << debug_iter.iteration << ":" << std::endl;
        std::cout << "  State: " << debug_iter.state_before.to_string() << std::endl;
        std::cout << "  Goal satisfied: " << (debug_iter.goal_satisfied ? "YES" : "NO") << std::endl;

        if (!debug_iter.applied_supporters.empty()) {
            std::cout << "  Applied " << debug_iter.applied_supporters.size()
                      << " supporters:" << std::endl;
            for (size_t idx : debug_iter.applied_supporters) {
                std::cout << "    " << supporters_[idx].to_string() << std::endl;
            }
        }
    }

    std::cout << "\nFinal result: Goal reachable = " << (goal_reached_ ? "YES" : "NO")
              << ", Iterations = " << iteration_count_ << std::endl;
}

std::string ARPG::to_string() const {
    std::string result = "ARPG {\n";
    result += "  Supporters: " + std::to_string(supporters_.size()) + "\n";
    result += "  Iterations: " + std::to_string(iteration_count_) + "\n";
    result += "  Goal reachable: " + std::string(goal_reached_ ? "true" : "false") + "\n";
    result += "}";
    return result;
}

} // namespace rantanplan