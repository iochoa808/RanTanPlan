#pragma once

#include "../problem/problem.hpp"
#include "../problem/object.hpp"
#include "../problem/expr_pool.hpp"
#include "z3_variable_factory.hpp"
#include <z3++.h>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace rantanplan {

// Forward declarations
class Fluent;

/**
 * @brief Converts ExprID expressions to Z3 formulas using grounded variables
 *
 * Creates individual Z3 variables for each grounded fluent at specific timesteps.
 * Works in conjunction with GroundedEncoder to maintain consistency in variable
 * naming and creation.
 *
 * Key features:
 * - Fluent applications become individual variables (e.g., "located_plane1_city0_5")
 * - Proper type support for Boolean, Integer, Real, and Object fluents
 * - Uses Z3VariableFactory for consistent variable creation and naming
 * - Supports temporal encoding through timestep parameters
 *
 * All conversion methods throw std::runtime_error on malformed expressions.
 */
class GroundedEncodingVisitor {
private:
    z3::context& ctx_;
    const Problem* problem_;
    int current_timestep_;
    Z3VariableFactory* variable_factory_;

public:
    // Constructor
    GroundedEncodingVisitor(z3::context& ctx, const Problem* problem,
                           Z3VariableFactory* factory);

    // Temporal encoding methods
    void set_timestep(int timestep) { current_timestep_ = timestep; }
    int get_timestep() const { return current_timestep_; }
    void clear_timestep() { current_timestep_ = -1; }

    // ExprID-based conversion: walks ExprNode directly via ExprPool, no Expression needed
    z3::expr convert_from_pool(ExprID id, int timestep = -1);

private:
    // Recursive helper for ExprID-based conversion
    z3::expr convert_node(ExprID id);
    // Helper methods for specific Z3 operations
    z3::expr handle_and(const std::vector<z3::expr>& args);
    z3::expr handle_or(const std::vector<z3::expr>& args);
    z3::expr handle_not(const std::vector<z3::expr>& args);
    z3::expr handle_equals(const std::vector<z3::expr>& args);
    z3::expr handle_less_than(const std::vector<z3::expr>& args);
    z3::expr handle_less_equal(const std::vector<z3::expr>& args);
    z3::expr handle_greater_than(const std::vector<z3::expr>& args);
    z3::expr handle_greater_equal(const std::vector<z3::expr>& args);
    z3::expr handle_plus(const std::vector<z3::expr>& args);
    z3::expr handle_minus(const std::vector<z3::expr>& args);
    z3::expr handle_multiply(const std::vector<z3::expr>& args);
    z3::expr handle_divide(const std::vector<z3::expr>& args);
    z3::expr handle_implies(const std::vector<z3::expr>& args);
};

} // namespace rantanplan
