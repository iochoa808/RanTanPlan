#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <optional>
#include <iostream>
#include <limits>
#include <cmath>
#include "../problem/problem.hpp"
#include "../problem/action.hpp"
#include "../problem/effect.hpp"
#include "../problem/expression.hpp"

namespace planmt {

/**
 * @brief Interval class for interval arithmetic operations
 *
 * Represents closed intervals [lower, upper] with support for arithmetic operations.
 * Based on the interval operations described in the ARPG paper (Section 3.1.1).
 */
class Interval {
public:
    // Constructors
    Interval() : lower_(-std::numeric_limits<double>::infinity()),
                 upper_(std::numeric_limits<double>::infinity()) {}

    Interval(double value) : lower_(value), upper_(value) {}

    Interval(double lower, double upper) : lower_(lower), upper_(upper) {
        if (lower > upper) {
            std::swap(lower_, upper_);
        }
    }

    // Accessors
    double lower() const { return lower_; }
    double upper() const { return upper_; }
    bool is_empty() const { return lower_ > upper_; }
    bool is_unbounded() const {
        return std::isinf(lower_) || std::isinf(upper_);
    }

    // Get the midpoint of the interval
    double midpoint() const {
        if (is_unbounded()) {
            // For infinite intervals, return a reasonable default
            if (std::isinf(lower_) && std::isinf(upper_)) {
                return 0.0;  // (-∞, +∞) -> 0
            } else if (std::isinf(lower_) && lower_ < 0) {
                return upper_ - 1.0;  // (-∞, x) -> x-1
            } else if (std::isinf(upper_) && upper_ > 0) {
                return lower_ + 1.0;  // (x, +∞) -> x+1
            }
        }
        return (lower_ + upper_) / 2.0;
    }

    // Containment checks
    bool contains(double value) const {
        return value >= lower_ && value <= upper_;
    }

    bool contains(const Interval& other) const {
        return lower_ <= other.lower_ && upper_ >= other.upper_;
    }

    // Interval arithmetic operations (as per paper Section 3.1.1)
    Interval operator+(const Interval& other) const {
        return Interval(lower_ + other.lower_, upper_ + other.upper_);
    }

    Interval operator-(const Interval& other) const {
        return Interval(lower_ - other.upper_, upper_ - other.lower_);
    }

    Interval operator*(const Interval& other) const {
        double ll = lower_ * other.lower_;
        double lu = lower_ * other.upper_;
        double ul = upper_ * other.lower_;
        double uu = upper_ * other.upper_;

        return Interval(std::min({ll, lu, ul, uu}), std::max({ll, lu, ul, uu}));
    }

    // Convex union operation (Definition 4 in paper)
    Interval convex_union(const Interval& other) const {
        return Interval(std::min(lower_, other.lower_), std::max(upper_, other.upper_));
    }

    // String representation
    std::string to_string() const {
        if (is_empty()) {
            return "[]";
        }
        return "[" + std::to_string(lower_) + ", " + std::to_string(upper_) + "]";
    }

    // Comparison operators
    bool operator==(const Interval& other) const {
        return lower_ == other.lower_ && upper_ == other.upper_;
    }

    bool operator!=(const Interval& other) const {
        return !(*this == other);
    }

private:
    double lower_;
    double upper_;
};

/**
 * @brief RelaxedState represents a state in the interval-based relaxation
 *
 * Maps numeric variables to intervals and tracks boolean (propositional) variables.
 * In the relaxed semantics, boolean variables accumulate monotonically.
 * Corresponds to s+ in the ARPG paper.
 */
class RelaxedState {
public:
    RelaxedState() = default;

    // Numeric variable management
    void set_variable(const std::string& var_name, const Interval& interval) {
        variable_intervals_[var_name] = interval;
    }

    // Get interval for a variable (returns nullopt if not found)
    std::optional<Interval> get_variable(const std::string& var_name) const {
        auto it = variable_intervals_.find(var_name);
        if (it != variable_intervals_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Check if numeric variable is defined
    bool has_variable(const std::string& var_name) const {
        return variable_intervals_.find(var_name) != variable_intervals_.end();
    }

    // Boolean (propositional) variable management
    void set_proposition(const std::string& prop_name, bool value = true) {
        if (value) {
            true_propositions_.insert(prop_name);
        } else {
            true_propositions_.erase(prop_name);
        }
    }

    // Add proposition to the true set (monotonic accumulation)
    void add_proposition(const std::string& prop_name) {
        true_propositions_.insert(prop_name);
    }

    // Check if proposition is satisfied (true) in the relaxed state
    bool is_proposition_true(const std::string& prop_name) const {
        return true_propositions_.find(prop_name) != true_propositions_.end();
    }

    // Get all true propositions
    const std::unordered_set<std::string>& get_true_propositions() const {
        return true_propositions_;
    }

    // Get all variable names
    std::vector<std::string> get_variable_names() const {
        std::vector<std::string> names;
        for (const auto& pair : variable_intervals_) {
            names.push_back(pair.first);
        }
        return names;
    }

    // Apply convex union to a variable's interval
    void extend_variable(const std::string& var_name, const Interval& new_interval) {
        if (has_variable(var_name)) {
            variable_intervals_[var_name] = variable_intervals_[var_name].convex_union(new_interval);
        } else {
            set_variable(var_name, new_interval);
        }
    }

    // Evaluate expressions to intervals (simplified from original 60+ line method)
    Interval evaluate_expression(const Expression& expr) const {
        // Handle constants
        if (expr.is_constant() && expr.is_atom()) {
            const auto& atom = expr.value();
            if (atom.is_boolean()) return Interval(atom.boolean() ? 1.0 : 0.0);
            if (atom.is_real()) return Interval(atom.real().to_double());
            if (atom.is_integer()) return Interval(static_cast<double>(atom.integer()));
        }

        // Handle variables by name lookup
        if (expr.is_parameter() || expr.is_variable() || expr.is_state_variable() ||
            expr.is_fluent_symbol() || expr.is_atom()) {
            std::string var_name = expr.to_string();
            auto result = get_variable(var_name);
            return result.value_or(Interval(0.0));
        }

        // Handle arithmetic operations
        if (expr.is_plus() && expr.list_size() == 2) {
            return evaluate_expression(expr.list_element(0)) + evaluate_expression(expr.list_element(1));
        }
        if (expr.is_minus() && expr.list_size() == 2) {
            return evaluate_expression(expr.list_element(0)) - evaluate_expression(expr.list_element(1));
        }
        if (expr.is_multiply() && expr.list_size() == 2) {
            Interval left = evaluate_expression(expr.list_element(0));
            Interval right = evaluate_expression(expr.list_element(1));
            // Handle missing variable special case
            if ((left.lower() == 0.0 && left.upper() == 0.0 && right.upper() > 0.0) ||
                (right.lower() == 0.0 && right.upper() == 0.0 && left.upper() > 0.0)) {
                double nonzero_val = (left.upper() > 0.0) ? left.upper() : right.upper();
                return Interval(nonzero_val);
            }
            return left * right;
        }

        return Interval(1.0); // Default for unknown expressions
    }

    // Check if conditions are satisfied (simplified from original 40+ line method)
    bool satisfies_condition(const Expression& condition) const {
        // Handle boolean constants
        if (condition.is_constant() && condition.is_atom() && condition.value().is_boolean()) {
            return condition.value().boolean();
        }

        // Handle propositions
        if (condition.is_state_variable() || condition.is_fluent_symbol()) {
            std::string condition_name = condition.to_string();

            // First check if it exists as a boolean proposition
            if (is_proposition_true(condition_name)) {
                return true;
            }

            // If not found as proposition, check if it exists as a numeric interval
            // This handles cases like location predicates that may be stored as intervals
            auto interval_opt = get_variable(condition_name);
            if (interval_opt.has_value()) {
                // Consider the condition satisfied if the interval has a positive upper bound
                return interval_opt->upper() > 0.0;
            }

            return false;
        }

        // Handle logical operations
        if (condition.is_and()) {
            for (size_t i = 0; i < condition.list_size(); ++i) {
                if (!satisfies_condition(condition.list_element(i))) return false;
            }
            return true;
        }
        if (condition.is_or()) {
            for (size_t i = 0; i < condition.list_size(); ++i) {
                if (satisfies_condition(condition.list_element(i))) return true;
            }
            return false;
        }
        if (condition.is_not() && condition.list_size() == 1) {
            return !satisfies_condition(condition.list_element(0));
        }

        // Handle numeric comparisons
        if (condition.list_size() >= 2) {
            // For expressions like (<= a b), the operands are typically at indices 0 and 1
            // But if list_size() is 3, it might be operator at 0 and operands at 1,2
            size_t left_idx = (condition.list_size() == 3) ? 1 : 0;
            size_t right_idx = (condition.list_size() == 3) ? 2 : 1;

            Interval left = evaluate_expression(condition.list_element(left_idx));
            Interval right = evaluate_expression(condition.list_element(right_idx));

            if (condition.is_greater_equal()) {
                return (left - right).lower() >= 0.0;
            }
            if (condition.is_greater_than()) {
                return (left - right).lower() > 0.0;
            }
            if (condition.is_less_equal()) {
                // For relaxed planning: A <= B is satisfied if it's possible that A <= B
                // This means A.lower() <= B.upper()
                return left.lower() <= right.upper();
            }
            if (condition.is_less_than()) {
                // For relaxed planning: A < B is satisfied if it's possible that A < B
                // This means A.lower() < B.upper()
                return left.lower() < right.upper();
            }
            if (condition.is_equals()) {
                return !(left.upper() < right.lower() || right.upper() < left.lower());
            }
        }

        return true; // Default assume satisfied
    }

    // Simplified string representation
    std::string to_string() const {
        std::string result = "RelaxedState{vars:" + std::to_string(variable_intervals_.size()) +
                           ", props:" + std::to_string(true_propositions_.size()) + "}";
        return result;
    }

private:
    std::unordered_map<std::string, Interval> variable_intervals_;
    std::unordered_set<std::string> true_propositions_;
};

/**
 * @brief Supporter represents a transformed action effect for ARPG construction
 *
 * Simplified implementation that directly stores the effect and computes
 * the appropriate interval transformation when applied.
 */
class Supporter {
public:
    // Constructor with action and effect
    Supporter(const std::string& name, const std::string& affected_variable,
              const Action& source_action, const Effect& effect);

    // Accessors
    const std::string& name() const { return name_; }
    const std::string& affected_variable() const { return affected_variable_; }
    const Action& source_action() const { return source_action_; }
    const Effect& effect() const { return effect_; }

    // Check if this supporter is applicable in a relaxed state
    bool is_applicable(const RelaxedState& state) const;

    // Apply this supporter's effect to a relaxed state
    void apply_effect(RelaxedState& state) const;

    // String representation for debugging
    std::string to_string() const;

private:
    std::string name_;
    std::string affected_variable_;
    const Action& source_action_;
    const Effect& effect_;

    // Compute the interval effect based on the effect type and current value
    Interval compute_effect_interval(double current_value) const;
};

/**
 * @brief Asymptotic Relaxed Planning Graph implementation
 *
 * Simplified implementation based on ARPG construction (Algorithm 1).
 * Uses efficient single-pass construction without complex layer management.
 */
class ARPG {
public:
    // Constructor
    ARPG(const Problem& problem);

    // Main construction algorithm
    bool construct_graph();

    // Check if goal is reachable
    bool is_goal_reachable() const { return goal_reached_; }

    // Get the final relaxed state
    const RelaxedState& get_final_state() const { return current_state_; }

    // Get bounds for all state variables from the final relaxed state
    std::unordered_map<Expression, Interval> get_state_variable_bounds() const;

    // Get construction statistics
    size_t get_num_supporters() const { return supporters_.size(); }
    int get_num_iterations() const { return iteration_count_; }

    // Action ordering support - expose supporter ordering information
    struct SupporterOrderingInfo {
        const Supporter& supporter;
        int iteration;
        const Action& source_action;

        SupporterOrderingInfo(const Supporter& supp, int iter, const Action& action)
            : supporter(supp), iteration(iter), source_action(action) {}
    };

    // Get supporters in the order they were applied during ARPG construction
    std::vector<SupporterOrderingInfo> get_supporter_ordering() const;

    // Get actions ordered by their first appearance in ARPG construction
    std::vector<const Action*> get_action_ordering() const;

    // Debug output - keep for debugging issues
    void print_construction_steps() const;
    std::string to_string() const;

private:
    const Problem& problem_;
    RelaxedState current_state_;
    std::vector<Supporter> supporters_;
    std::vector<bool> used_supporters_;
    bool goal_reached_;
    int iteration_count_;

    // Debug info for construction steps
    struct DebugIteration {
        int iteration;
        RelaxedState state_before;
        std::vector<size_t> applied_supporters;
        bool goal_satisfied;
    };
    std::vector<DebugIteration> debug_iterations_;

    // Create supporters from actions
    void create_supporters();

    // Find applicable supporters that haven't been used
    std::vector<size_t> find_applicable_supporters() const;

    // Apply supporters by indices
    void apply_supporters(const std::vector<size_t>& indices);

    // Check if goal conditions are satisfied
    bool check_goal_satisfaction() const;

    // Initialize relaxed state from problem initial state
    RelaxedState create_initial_state() const;
};

} // namespace planmt