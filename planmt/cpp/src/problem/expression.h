#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "protobuf_aliases.h"
#include "atom.h"

namespace planmt {

/**
 * @brief Expression
 * 
 * Represents either an atom or a list of sub-expressions.
 */
class Expression {
public:
    // Expression kinds
    enum class Kind {
        UNKNOWN = 0,             // Unknown kind
        CONSTANT = 1,            // Constant atom, for example "true", "false", "42"
        PARAMETER = 2,           // Parameter from outer scope, for example "?x"of type robot from the action 
        VARIABLE = 3,            // Variable from outer scope, for example "?y" from a quantifier
        FLUENT_SYMBOL = 4,       // Fluent symbol, for example "at", "fuel_level"
        FUNCTION_SYMBOL = 5,     // Function symbol, for example "+", "-", "<=", ">", "=", "and", "or"
        STATE_VARIABLE = 6,      // Fluent application, for example "at(robot1, loc1)", "fuel_level(robot1)"
        FUNCTION_APPLICATION = 7 // Function application, for example "(+ 1 2)", "(and (at robot1 loc1) (at robot2 loc2))"
    };
    
    // Constructors
    Expression() : kind_(Kind::UNKNOWN) {}
    Expression(const pb::Expression& pb_expression);
    
    // checkers
    bool is_atom() const { return atom_.has_value() && list_.empty(); }
    bool is_list() const { return !list_.empty(); }
    
    // getters
    const Atom& value() const { return atom_.value(); }

    Kind kind() const { return kind_; }
    const std::string& type() const { return type_; }

    size_t list_size() const { return list_.size(); }
    const std::vector<Expression>& list() const { return list_; }
    const Expression& list_element(size_t index) const { return list_[index]; }
    
    // Convenience methods for common expression types
    bool is_constant() const { return kind_ == Kind::CONSTANT; }
    bool is_parameter() const { return kind_ == Kind::PARAMETER; }
    bool is_variable() const { return kind_ == Kind::VARIABLE; }
    bool is_fluent_symbol() const { return kind_ == Kind::FLUENT_SYMBOL; }
    bool is_function_symbol() const { return kind_ == Kind::FUNCTION_SYMBOL; }
    bool is_state_variable() const { return kind_ == Kind::STATE_VARIABLE; }
    bool is_function_application() const { return kind_ == Kind::FUNCTION_APPLICATION; }
    
    // Convenience methods for common operators
    bool is_and() const;
    bool is_or() const;
    bool is_not() const;
    bool is_implies() const;
    bool is_iff() const;
    bool is_exists() const;
    bool is_forall() const;
    bool is_equals() const;
    bool is_not_equals() const;
    bool is_plus() const;
    bool is_minus() const;
    bool is_multiply() const;
    bool is_divide() const;
    bool is_less_than() const;
    bool is_less_equal() const;
    bool is_greater_than() const;
    bool is_greater_equal() const;
    
    // String representation
    std::string to_string() const;
    
    // Operators
    bool operator==(const Expression& other) const;
    bool operator!=(const Expression& other) const { return !(*this == other); }

private:
    std::optional<Atom> atom_;
    std::vector<Expression> list_;

    Kind kind_;        // structural information (such as whether it is a function application, atom, etc.)
    std::string type_; // for example, "up:bool", "up:integer" or custom such as "location", "robot", etc ...
};

} // namespace planmt
