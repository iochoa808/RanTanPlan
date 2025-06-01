#pragma once

#include "../problem/visitors/expression_visitor.h"
#include "../problem/problem.h"
#include <z3++.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <memory>

namespace planmt {

// Type alias for the unified symbol table
using Z3Object = std::variant<z3::expr, z3::func_decl>;
using SymbolTable = std::unordered_map<std::string, Z3Object>;

/**
 * @brief Visitor that converts expressions to Z3 formulas
 * 
 * Converts expressions containing atoms and function applications to Z3 expressions.
 * Handles integers, reals, booleans, symbols, and common arithmetic operations.
 * Uses external symbol table provided by GroundedEncoder.
 * Supports temporal encoding through timestep parameters.
 * 
 * TODO: properly check this with an ungrounded problem instance.
 */

class LiftedEncodingVisitor : public BaseExpressionVisitor {
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
    
    // BaseExpressionVisitor interface methods
    void visit_symbol(const std::string& symbol, Expression::Kind kind, const Type* type) override;
    void visit_integer(int64_t value, Expression::Kind kind) override;
    void visit_real(const Real& value, Expression::Kind kind) override;
    void visit_boolean(bool value, Expression::Kind kind) override;
    void visit_function_application(const std::string& function_name, 
                                  const std::vector<Expression>& args,
                                  Expression::Kind kind) override;
    void visit_fluent_application(const std::string& fluent_name,
                                const std::vector<Expression>& args,
                                Expression::Kind kind) override;
    void visit_list(const std::vector<Expression>& elements, 
                   Expression::Kind kind) override;
    
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

    // Public methods for creating variables from names (to be used by GroundedEncoder)
    z3::expr create_bool_variable(const std::string& name);
    z3::expr create_int_variable(const std::string& name);
    z3::expr create_real_variable(const std::string& name);
    
private:
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
    
    // Helper to determine Z3 sort for a symbol based on its kind
    z3::sort get_sort_for_symbol(const std::string& symbol, Expression::Kind kind);
};

} // namespace planmt
