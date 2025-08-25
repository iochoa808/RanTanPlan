#include "supporter.h"
#include "relaxed_state.h"

namespace planmt {

bool Supporter::is_applicable(const RelaxedState& state) const {
    // Check if all preconditions are satisfied in the relaxed state
    for (const auto& precond : preconditions_) {
        if (!state.satisfies_condition(precond)) {
            return false;
        }
    }
    return true;
}

void Supporter::apply_effect(RelaxedState& state) const {
    switch (effect_type_) {
        case EffectType::POSITIVE_INFINITY:
        case EffectType::NEGATIVE_INFINITY:
        case EffectType::CONSTANT_ASSIGNMENT: {
            // Get current variable value (using midpoint of current interval)
            auto interval_opt = state.get_variable(affected_variable_);
            Interval current_interval = interval_opt.value_or(Interval(0.0));
            double current_value = current_interval.midpoint();
            
            // Apply interval effect relative to current value (Definition 9)
            Interval effect_interval = get_effect_interval(current_value);
            state.extend_variable(affected_variable_, effect_interval);
            break;
        }
        case EffectType::BOOLEAN_ADD: {
            // Add proposition to true set (monotonic relaxation)
            state.add_proposition(affected_variable_);
            break;
        }
        case EffectType::BOOLEAN_DELETE: {
            // In relaxed planning, we typically don't remove propositions
            // but for completeness, this could be implemented
            // (would require more sophisticated relaxation handling)
            break;
        }
    }
}

} // namespace planmt