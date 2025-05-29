#pragma once

#include <string>
#include <vector>
#include <memory>
#include "protobuf_aliases.h"
#include "atom.h"

namespace planmt {

/**
 * @brief Simple expression representation
 * 
 * Represents either an atom or a list of sub-expressions.
 * Much simpler than protobuf Expression - focuses on essential functionality.
 * Omits temporal planning features.
 */
class Expression {
public:
    // Expression kinds (simplified from protobuf)
    enum class Kind {
        UNKNOWN = 0,
        CONSTANT = 1,        // Constant atom
        PARAMETER = 2,       // Parameter from outer scope
        VARIABLE = 3,        // Variable from outer scope
        FLUENT_SYMBOL = 4,   // Fluent symbol
        FUNCTION_SYMBOL = 5, // Function symbol
        STATE_VARIABLE = 6,  // Fluent application
        FUNCTION_APPLICATION = 7 // Function application
    };
    
    // Constructors
    Expression() : kind_(Kind::UNKNOWN) {}
    Expression(const Atom& atom, Kind kind = Kind::CONSTANT) 
        : atom_(atom), kind_(kind) {}
    Expression(const std::vector<Expression>& list, Kind kind = Kind::FUNCTION_APPLICATION)
        : list_(list), kind_(kind) {}
    Expression(const pb::Expression& pb_expression);
    
    // Type checkers
    bool is_atom() const { return atom_.has_value() && list_.empty(); }
    bool is_list() const { return !list_.empty(); }
    
    // Value getters
    const Atom& atom() const { return atom_.value(); }
    const std::vector<Expression>& list() const { return list_; }
    Kind kind() const { return kind_; }
    const std::string& type() const { return type_; }
    
    // Setters
    void set_atom(const Atom& atom) { atom_ = atom; list_.clear(); }
    void set_list(const std::vector<Expression>& list) { list_ = list; atom_.reset(); }
    void set_kind(Kind kind) { kind_ = kind; }
    void set_type(const std::string& type) { type_ = type; }
    
    // List manipulation
    void add_expression(const Expression& expr) { list_.push_back(expr); }
    size_t list_size() const { return list_.size(); }
    const Expression& list_element(size_t index) const { return list_[index]; }
    
    // Convenience methods for common expression types
    bool is_constant() const { return kind_ == Kind::CONSTANT; }
    bool is_parameter() const { return kind_ == Kind::PARAMETER; }
    bool is_variable() const { return kind_ == Kind::VARIABLE; }
    bool is_fluent_symbol() const { return kind_ == Kind::FLUENT_SYMBOL; }
    bool is_function_symbol() const { return kind_ == Kind::FUNCTION_SYMBOL; }
    bool is_state_variable() const { return kind_ == Kind::STATE_VARIABLE; }
    bool is_function_application() const { return kind_ == Kind::FUNCTION_APPLICATION; }
    
    // String representation
    std::string to_string() const;
    
    // Convert to protobuf Expression
    pb::Expression to_protobuf() const;
    
    // Operators
    bool operator==(const Expression& other) const;
    bool operator!=(const Expression& other) const { return !(*this == other); }

private:
    std::optional<Atom> atom_;
    std::vector<Expression> list_;
    Kind kind_;
    std::string type_;
    
    // Helper for string conversion
    std::string list_to_string() const;
};

} // namespace planmt
