#pragma once

#include "../problem/visitors/expression_visitor.h"
#include <z3++.h>
#include <optional>
#include <unordered_map>
#include <string>
#include <memory>
#include <stdexcept>

namespace planmt {

/**
 * @brief Visitor that converts expressions to Z3 formulas
 * 
 * Converts expressions containing atoms and function applications to Z3 expressions.
 * Handles integers, reals, booleans, symbols, and common arithmetic operations.
 * Maintains a cache of symbol-to-Z3 variable mappings for consistency.
 */
class SmtEncodingVisitor : public BaseExpressionVisitor {
public:
    // Type alias for symbol cache
    using SymbolCache = std::unordered_map<std::string, std::shared_ptr<z3::expr>>;
    
private:
    z3::context& ctx_;
    std::optional<z3::expr> result_;
    SymbolCache& symbol_cache_;
    
public:
    // Constructor - cache must be provided externally
    SmtEncodingVisitor(z3::context& ctx, SymbolCache& symbol_cache) 
        : ctx_(ctx), symbol_cache_(symbol_cache) {}
    
    // Visitor methods
    void visit_symbol(const std::string& symbol, Expression::Kind kind) override;
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
    
    void clear_cache() {
        symbol_cache_.clear();
    }
    
    // Get access to the symbol cache for sharing
    SymbolCache& get_symbol_cache() { return symbol_cache_; }
    const SymbolCache& get_symbol_cache() const { return symbol_cache_; }

private:
    // Helper methods for arithmetic operations
    void encode_addition(const std::vector<Expression>& args);
    void encode_subtraction(const std::vector<Expression>& args);
    void encode_multiplication(const std::vector<Expression>& args);
    void encode_division(const std::vector<Expression>& args);
    
    // Helper methods for logical operations
    void encode_and(const std::vector<Expression>& args);
    void encode_or(const std::vector<Expression>& args);
    void encode_not(const std::vector<Expression>& args);
    void encode_implies(const std::vector<Expression>& args);
    void encode_equals(const std::vector<Expression>& args);
    void encode_less_than(const std::vector<Expression>& args);
    void encode_less_equal(const std::vector<Expression>& args);
    void encode_greater_than(const std::vector<Expression>& args);
    void encode_greater_equal(const std::vector<Expression>& args);
    
    // Helper to get or create Z3 variable for symbol
    z3::expr get_or_create_variable(const std::string& symbol, Expression::Kind kind);
    
    // Helper to determine Z3 sort for a symbol based on its kind
    z3::sort get_sort_for_symbol(const std::string& symbol, Expression::Kind kind);
};

} // namespace planmt
