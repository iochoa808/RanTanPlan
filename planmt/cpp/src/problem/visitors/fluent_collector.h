#pragma once

#include "expression_visitor.h"
#include <unordered_set>
#include <string>
#include <vector>

namespace planmt {

/**
 * @brief Visitor that collects all fluents in an expression
 * 
 * Traverses the expression tree and collects all fluent applications
 * (state variables) that appear in the expression. This is useful for
 * analyzing dependencies in preconditions and effects.
 */
class FluentCollector : public BaseExpressionVisitor {
private:
    std::unordered_set<Expression> fluents_;
    
    /**
     * @brief Collects a fluent expression
     */
    void collect_fluent(const Expression& fluent_expr) {
        fluents_.insert(fluent_expr);
    }
    
public:
    void visit_symbol(const std::string& symbol, Expression::Kind kind, const Type* type) override {
        // Single fluent symbols (no arguments) - handled by collect_from_expression
    }
    
    void visit_function_application(const std::string& function_name, 
                                  const std::vector<Expression>& args,
                                  Expression::Kind kind) override {
        // Recursively visit all arguments to find nested fluents
        for (const auto& arg : args) {
            accept_visitor(arg, *this);
        }
    }
    
    void visit_fluent_application(const std::string& fluent_name,
                                const std::vector<Expression>& args,
                                Expression::Kind kind) override {
        // Recursively visit arguments in case they contain nested fluents
        for (const auto& arg : args) {
            accept_visitor(arg, *this);
        }
    }
    
    void visit_list(const std::vector<Expression>& elements, 
                   Expression::Kind kind) override {
        // Recursively visit all elements
        for (const auto& element : elements) {
            accept_visitor(element, *this);
        }
    }
    
    /**
     * @brief Main entry point to collect fluents from an expression
     * 
     * Call this method to collect all fluents from a complete expression.
     * This handles the full expression context that individual visit methods lack.
     */
    void collect_from_expression(const Expression& expr) {
        // Check if this expression itself is a fluent
        if (expr.kind() == Expression::Kind::FLUENT_SYMBOL || 
            expr.kind() == Expression::Kind::STATE_VARIABLE) {
            collect_fluent(expr);
        }
        
        // Traverse the expression tree to find nested fluents
        accept_visitor(expr, *this);
    } 
    
    // Accessors
    const std::unordered_set<Expression>& get_fluents() const { return fluents_; }
    
    // Utility methods
    bool has_fluents() const { return !fluents_.empty(); }
    size_t fluent_count() const { return fluents_.size(); }
    
    // Clear all collected fluents
    void clear() { fluents_.clear(); }
    
    // Get string representation of all collected fluents
    std::string to_string() const {
        std::string result = "Collected fluents:\n";
        for (const auto& fluent : fluents_) {
            result += "  " + fluent.to_string() + "\n";
        }
        return result;
    }
};

} // namespace planmt
