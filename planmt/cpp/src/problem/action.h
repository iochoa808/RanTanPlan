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
    Action(const pb::Action& pb_action, const std::vector<Parameter>& parameters, const Problem* problem);
    
    // Basic accessors
    const std::string& name() const { return name_; }
    
    // Parameter access
    const std::vector<Parameter>& parameters() const { return parameters_; }
    size_t parameter_count() const { return parameters_.size(); }
    const Parameter& parameter(size_t index) const { return parameters_[index]; }
    bool has_parameter(const std::string& name) const;
    const Parameter* find_parameter(const std::string& name) const;
    
    // Precondition access
    const Expression& precondition() const { return precondition_; }
    bool has_precondition() const { 
        return !(precondition_.is_constant() && precondition_.is_atom() && 
                 precondition_.value().is_boolean() && precondition_.value().boolean() == true); 
    }
    
    // Effect access
    const std::vector<Effect>& effects() const { return effects_; }
    size_t effect_count() const { return effects_.size(); }
    const Effect& effect(size_t index) const { return effects_[index]; }
    
    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void add_parameter(const Parameter& param);
    void set_parameters(const std::vector<Parameter>& parameters);
    void set_precondition(const Expression& precond) { precondition_ = precond; }
    void add_effect(const Effect& effect) { effects_.push_back(effect); }
    void set_effects(const std::vector<Effect>& effects) { effects_ = effects; }
    
    // String representation
    std::string to_string() const;
    
    // Operators
    bool operator==(const Action& other) const;
    bool operator!=(const Action& other) const { return !(*this == other); }
    bool operator<(const Action& other) const;

private:
    std::string name_;
    std::vector<Parameter> parameters_;
    Expression precondition_;
    std::vector<Effect> effects_;
    
    // Quick lookup for parameters
    std::unordered_map<std::string, size_t> parameter_name_to_index_;
    
    void build_parameter_mappings();
};

} // namespace planmt

// specialising std::hash for Action by using its string representation
// note that for this to work we need to be in the std namespace
namespace std {
    template<>
    struct hash<planmt::Action> {
        std::size_t operator()(const planmt::Action& action) const {
            std::size_t h1 = std::hash<std::string>()(action.name());
            std::size_t h2 = action.parameter_count();
            return h1 ^ (h2 << 1);
        }
    };
}