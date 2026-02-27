#pragma once

#include <string>
#include <vector>
#include <optional>
#include "protobuf_aliases.hpp"
#include "parameter.hpp"
#include "type.hpp"

namespace rantanplan {

/**
 * @brief Fluent
 * 
 * Represents a state-dependent variable (fluent) in the planning domain.
 */
class Fluent {
public:
    // Constructors
    Fluent() : id_(-1) {}
    Fluent(const std::string& name, const Type* value_type)
        : name_(name), value_type_(value_type), id_(-1) {}
    Fluent(const std::string& name, const Type* value_type,
           const std::vector<Parameter>& parameters)
        : name_(name), value_type_(value_type), parameters_(parameters), id_(-1) {}
    Fluent(const pb::Fluent& pb_fluent, const Type* value_type, const std::vector<Parameter>& parameters);
    
    // Accessors
    const std::string& name() const { return name_; }
    int id() const { return id_; }
    const Type* value_type() const { return value_type_; }
    const std::vector<Parameter>& parameters() const { return parameters_; }
    size_t parameter_count() const { return parameters_.size(); }
    const Parameter& parameter(size_t index) const { return parameters_[index]; }
    
    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void set_id(int id) { id_ = id; }
    void set_value_type(const Type* value_type) { value_type_ = value_type; }
    void add_parameter(const Parameter& param) { parameters_.push_back(param); }
    void set_parameters(const std::vector<Parameter>& parameters) { parameters_ = parameters; }
    
    // Convenience methods
    bool is_predicate() const {
        return value_type_ && value_type_->is_bool();
    }
    bool is_function() const {
        return value_type_ && !value_type_->is_bool();
    }
    
    // Type checking methods using the Type class
    bool is_bool_fluent() const {
        return value_type_ && value_type_->is_bool();
    }
    bool is_int_fluent() const {
        return value_type_ && value_type_->is_int();
    }
    bool is_real_fluent() const {
        return value_type_ && value_type_->is_real();
    }
    bool is_object_fluent() const {
        return value_type_ && value_type_->is_object();
    }
    
    // String representation
    std::string to_string() const;
    
    // Operators
    bool operator==(const Fluent& other) const;
    bool operator!=(const Fluent& other) const { return !(*this == other); }

private:
    std::string name_;
    int id_;
    const Type* value_type_ = nullptr;
    std::vector<Parameter> parameters_;
};

} // namespace rantanplan

// specialising std::hash for Fluent by using its ID
// note that for this to work we need to be in the std namespace
namespace std {
    template<>
    struct hash<rantanplan::Fluent> {
        std::size_t operator()(const rantanplan::Fluent& fluent) const {
            return std::hash<int>()(fluent.id());
        }
    };
}
