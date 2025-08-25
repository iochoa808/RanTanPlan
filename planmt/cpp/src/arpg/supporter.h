#pragma once

#include <string>
#include <vector>
#include "../problem/expression.h"
#include "../problem/effect.h"
#include "interval.h"

namespace planmt {

/**
 * @brief Supporter represents a transformed action for asymptotic analysis
 * 
 * As described in the ARPG paper (Definition 9), supporters model the asymptotic
 * behaviour of numeric effects. Each supporter has preconditions and an interval effect.
 */
class Supporter {
public:
    enum class EffectType {
        POSITIVE_INFINITY,  // lhs(e) = (x, +∞) when rhs(e) > 0
        NEGATIVE_INFINITY,  // lhs(e) = (-∞, x) when rhs(e) < 0
        CONSTANT_ASSIGNMENT,// lhs(e) = k (constant)
        BOOLEAN_ADD,        // Add proposition to true set
        BOOLEAN_DELETE      // Remove proposition from true set (for completeness)
    };
    
    
    // Constructor
    Supporter(const std::string& name, const std::string& affected_variable)
        : name_(name), affected_variable_(affected_variable) {}
    
    // Accessors
    const std::string& name() const { return name_; }
    const std::string& affected_variable() const { return affected_variable_; }
    EffectType effect_type() const { return effect_type_; }
    double constant_value() const { return constant_value_; }
    
    // Preconditions
    void add_precondition(const Expression& precondition) {
        preconditions_.push_back(precondition);
    }
    
    const std::vector<Expression>& preconditions() const {
        return preconditions_;
    }
    
    // Set effect type and value
    void set_positive_infinity_effect() {
        effect_type_ = EffectType::POSITIVE_INFINITY;
    }
    
    void set_negative_infinity_effect() {
        effect_type_ = EffectType::NEGATIVE_INFINITY;
    }
    
    void set_constant_effect(double value) {
        effect_type_ = EffectType::CONSTANT_ASSIGNMENT;
        constant_value_ = value;
    }
    
    void set_boolean_add_effect() {
        effect_type_ = EffectType::BOOLEAN_ADD;
    }
    
    void set_boolean_delete_effect() {
        effect_type_ = EffectType::BOOLEAN_DELETE;
    }
    
    // Get the interval effect this supporter applies relative to current variable value
    Interval get_effect_interval(double current_value) const {
        switch (effect_type_) {
            case EffectType::POSITIVE_INFINITY:
                // Definition 9: (x, +∞) where x is current variable value
                return Interval(current_value, std::numeric_limits<double>::infinity());
            case EffectType::NEGATIVE_INFINITY:
                // Definition 9: (-∞, x) where x is current variable value
                return Interval(-std::numeric_limits<double>::infinity(), current_value);
            case EffectType::CONSTANT_ASSIGNMENT:
                // For constant assignment, interval is just the constant value
                return Interval(constant_value_);
            default:
                return Interval(0.0);
        }
    }
    
    // Legacy method for backward compatibility
    Interval get_effect_interval() const {
        return get_effect_interval(0.0);
    }
    
    // Check if this supporter is applicable in a relaxed state
    bool is_applicable(const class RelaxedState& state) const;
    
    // Apply this supporter's effect to a relaxed state
    void apply_effect(class RelaxedState& state) const;
    
    
    // String representation
    std::string to_string() const {
        std::string result = "Supporter[" + name_ + "] ";
        result += affected_variable_ + " := ";
        
        switch (effect_type_) {
            case EffectType::POSITIVE_INFINITY:
                result += "(x, +∞)";
                break;
            case EffectType::NEGATIVE_INFINITY:
                result += "(-∞, x)";
                break;
            case EffectType::CONSTANT_ASSIGNMENT:
                result += std::to_string(constant_value_);
                break;
            case EffectType::BOOLEAN_ADD:
                result += "true";
                break;
            case EffectType::BOOLEAN_DELETE:
                result += "false";
                break;
        }
        
        if (!preconditions_.empty()) {
            result += " | preconditions: " + std::to_string(preconditions_.size());
        }
        
        return result;
    }

private:
    std::string name_;
    std::string affected_variable_;
    EffectType effect_type_ = EffectType::CONSTANT_ASSIGNMENT;
    double constant_value_ = 0.0;
    std::vector<Expression> preconditions_;
};

} // namespace planmt