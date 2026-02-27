#pragma once

#include "../problem/problem.hpp"
#include "../problem/expr_pool.hpp"
#include <z3++.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <memory>

namespace rantanplan {

// Type alias for the unified symbol table
using Z3Object = std::variant<z3::expr, z3::func_decl>;
using SymbolTable = std::unordered_map<std::string, Z3Object>;

/**
 * @brief Converts ExprID expressions to Z3 formulas using uninterpreted functions
 *
 * Creates uninterpreted Z3 functions for fluent applications (lifted encoding).
 * Used by SMTSymmetryChecker for symmetry detection via Z3 substitution.
 * Supports temporal encoding through timestep parameters.
 */
class LiftedEncodingVisitor {
private:
    z3::context& ctx_;
    std::optional<z3::expr> result_;
    SymbolTable& symbol_table_; // Reference to external symbol table
    const Problem* problem_; // Reference to problem instance for fluent type lookup
    int current_timestep_; // Current timestep for temporal encoding (-1 means no timestep)

public:
    // Constructor
    LiftedEncodingVisitor(z3::context& ctx, SymbolTable& symbol_table, const Problem* problem = nullptr)
        : ctx_(ctx), symbol_table_(symbol_table), problem_(problem), current_timestep_(-1) {}

    // Result access
    std::optional<z3::expr> get_result() const { return result_; }
    bool has_result() const { return result_.has_value(); }
    z3::expr get_expression() const { return result_.value(); }

    // Utility methods
    void clear() {
        result_.reset();
    }

    void clear_symbol_table() {
        symbol_table_.clear();
    }

    // Temporal encoding methods
    void set_timestep(int timestep) { current_timestep_ = timestep; }
    int get_timestep() const { return current_timestep_; }
    void clear_timestep() { current_timestep_ = -1; }

    // Public methods for creating variables from names
    z3::expr create_bool_variable(const std::string& name);
    z3::expr create_int_variable(const std::string& name);
    z3::expr create_real_variable(const std::string& name);

    // ExprID-based conversion: walks ExprNode directly via ExprPool, no Expression needed
    std::optional<z3::expr> convert_from_pool(ExprID id, int timestep = -1);

private:
    // Recursive helper for ExprID-based conversion
    std::optional<z3::expr> convert_node_pool(ExprID id);
    // Helper methods for specific Z3 operations
    std::optional<z3::expr> handle_and(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_or(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_not(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_equals(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_less_than(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_less_equal(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_greater_than(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_greater_equal(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_plus(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_minus(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_multiply(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_divide(const std::vector<z3::expr>& args);
    std::optional<z3::expr> handle_uninterpreted_function(const std::string& name, const std::vector<z3::expr>& args, const z3::sort& return_sort);
};

} // namespace rantanplan
