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
    // Expression kinds. For some reason the protobuf enum has weird numbering.
    enum class Kind {
        // Default value, should not be used. Drop it if we are sure to never need it.
        UNKNOWN = 0, 

        // Constant atom. For instance, `true`, `3` or `kitchen` (where `kitchen` is
        // an object defined in the problem)
        CONSTANT = 1, 

        // Atom symbol representing a parameter from an outer scope. For instance
        // `from` that would appear inside a `(move from to - location)` action.
        PARAMETER = 2,           

        // Atom symbol representing a variable from an outer scope.
        // This is typically used to represent the variables that are existentially or universally qualified in expressions.
        VARIABLE = 7,

        // Atom symbol representing a fluent of the problem. For instance `at-robot`.
        FLUENT_SYMBOL = 3,

        // Atom representing a function. For instance `+`, `=`, `and`, ...
        FUNCTION_SYMBOL = 4, 

        // List. Application of some parameters to a fluent symbol. For instance `(at-robot l1)` or `(battery-charged)`
        // The first element of the list must be a FLUENT_SYMBOL
        STATE_VARIABLE = 5,

        // List. The expression is the application of some parameters to a function. For instance `(+ 1 3)`.
        // The first element of the list must be a FUNCTION_SYMBOL
        FUNCTION_APPLICATION = 6
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
