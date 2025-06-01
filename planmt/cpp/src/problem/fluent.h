#pragma once

#include <string>
#include <vector>
#include <optional>
#include "protobuf_aliases.h"
#include "parameter.h"
#include "expression.h"

namespace planmt {

/**
 * @brief Fluent
 * 
 * Represents a state-dependent variable (fluent) in the planning domain.
 */
class Fluent {
public:
    // Constructors
    Fluent() = default;
    Fluent(const std::string& name, const std::string& value_type)
        : name_(name), value_type_(value_type) {}
    Fluent(const std::string& name, const std::string& value_type, 
           const std::vector<Parameter>& parameters)
        : name_(name), value_type_(value_type), parameters_(parameters) {}
    Fluent(const pb::Fluent& pb_fluent);
    
    // Accessors
    const std::string& name() const { return name_; }
    const std::string& value_type() const { return value_type_; }
    const std::vector<Parameter>& parameters() const { return parameters_; }
    size_t parameter_count() const { return parameters_.size(); }
    const Parameter& parameter(size_t index) const { return parameters_[index]; }
    
    bool has_default_value() const { return default_value_.has_value(); }
    const Expression& default_value() const { return default_value_.value(); }
    
    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void set_value_type(const std::string& value_type) { value_type_ = value_type; }
    void add_parameter(const Parameter& param) { parameters_.push_back(param); }
    void set_parameters(const std::vector<Parameter>& parameters) { parameters_ = parameters; }
    void set_default_value(const Expression& default_value) { default_value_ = default_value; }
    void clear_default_value() { default_value_.reset(); }
    
    // Convenience methods
    bool is_predicate() const { return value_type_ == "up:bool" || value_type_ == "bool"; }
    bool is_function() const { return !is_predicate(); }
    
    // Type enum access
    Expression::Type get_type_enum() const { return Expression::string_to_type(value_type_); }
    
    // String representation
    std::string to_string() const;
    
    // Operators
    bool operator==(const Fluent& other) const;
    bool operator!=(const Fluent& other) const { return !(*this == other); }

private:
    std::string name_;
    std::string value_type_;
    std::vector<Parameter> parameters_;
    std::optional<Expression> default_value_;
};

} // namespace planmt
