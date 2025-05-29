#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "protobuf_aliases.h"
#include "parameter.h"
#include "expression.h"
#include "effect.h"

namespace planmt {

/**
 * @brief Action
 * 
 * Represents a planning action with parameters, preconditions, and effects.
 */
class Action {
public:
    // Constructors
    Action() = default;
    Action(const std::string& name) : name_(name) {}
    Action(const pb::Action& pb_action);
    
    // Basic accessors
    const std::string& name() const { return name_; }
    
    // Parameter access
    const std::vector<Parameter>& parameters() const { return parameters_; }
    size_t parameter_count() const { return parameters_.size(); }
    const Parameter& parameter(size_t index) const { return parameters_[index]; }
    bool has_parameter(const std::string& name) const;
    const Parameter* find_parameter(const std::string& name) const;
    
    // Precondition access
    const std::vector<Expression>& preconditions() const { return preconditions_; }
    size_t precondition_count() const { return preconditions_.size(); }
    const Expression& precondition(size_t index) const { return preconditions_[index]; }
    
    // Effect access
    const std::vector<Effect>& effects() const { return effects_; }
    size_t effect_count() const { return effects_.size(); }
    const Effect& effect(size_t index) const { return effects_[index]; }
    
    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void add_parameter(const Parameter& param);
    void set_parameters(const std::vector<Parameter>& parameters);
    void add_precondition(const Expression& precond) { preconditions_.push_back(precond); }
    void set_preconditions(const std::vector<Expression>& preconditions) { preconditions_ = preconditions; }
    void add_effect(const Effect& effect) { effects_.push_back(effect); }
    void set_effects(const std::vector<Effect>& effects) { effects_ = effects; }
    
    // String representation
    std::string to_string() const;
    
    // Convert to protobuf Action
    pb::Action to_protobuf() const;
    
    // Operators
    bool operator==(const Action& other) const;
    bool operator!=(const Action& other) const { return !(*this == other); }

private:
    std::string name_;
    std::vector<Parameter> parameters_;
    std::vector<Expression> preconditions_;
    std::vector<Effect> effects_;
    
    // Quick lookup for parameters
    std::unordered_map<std::string, size_t> parameter_name_to_index_;
    
    void build_parameter_mappings();
};

} // namespace planmt
