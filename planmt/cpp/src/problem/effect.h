#pragma once

#include "protobuf_aliases.h"
#include "effect_expression.h"

namespace planmt {

/**
 * @brief Effect
 * 
 * Wraps an EffectExpression, to represent an effect in a planning problem.
 * Supports conditional and quantified effects.
 */
class Effect {
public:
    // Constructors
    Effect() = default;
    Effect(const EffectExpression& effect_expr) : effect_expr_(effect_expr) {}
    Effect(const pb::Effect& pb_effect);
    
    // Accessors
    const EffectExpression& effect_expression() const { return effect_expr_; }
    
    // Setters
    void set_effect_expression(const EffectExpression& effect_expr) { effect_expr_ = effect_expr; }
    
    // Convenience methods that delegate to the effect expression
    EffectExpression::Kind kind() const { return effect_expr_.kind(); }
    const Expression& fluent() const { return effect_expr_.fluent(); }
    const Expression& value() const { return effect_expr_.value(); }
    bool has_condition() const { return effect_expr_.has_condition(); }
    const Expression& condition() const { return effect_expr_.condition(); }
    bool is_conditional() const { return effect_expr_.is_conditional(); }
    bool is_quantified() const { return effect_expr_.is_quantified(); }
    
    // String representation
    std::string to_string() const { return effect_expr_.to_string(); }
    
    // Operators
    bool operator==(const Effect& other) const { return effect_expr_ == other.effect_expr_; }
    bool operator!=(const Effect& other) const { return !(*this == other); }

private:
    EffectExpression effect_expr_;
};

} // namespace planmt
