#include "interference_analysis.h"

namespace planmt {


void InterferenceAnalysis::analyze_all_actions() {
    if (!problem_) {
        return;
    }
    
    // Clear any existing analysis
    action_analysis_.clear();
    
    // Analyze each action and store the results
    for (const Action& action : problem_->actions()) {
        action_analysis_[action] = analyze_action(action);
    }
}

InterferenceAnalysis::ActionAnalysis InterferenceAnalysis::analyze_action(const Action& action) const {
    ActionAnalysis analysis;
    // Analyze preconditions and effects
    analyze_preconditions(action, analysis);
    analyze_effects(action, analysis);
    return analysis;
}

void InterferenceAnalysis::analyze_preconditions(const Action& action, ActionAnalysis& analysis) const {
    if (!action.has_precondition()) {
        return;
    }
    
    // Use the fluent polarity collector to analyze preconditions and split by polarity
    FluentPolarityCollector collector;
    collector.collect_from_expression(action.precondition());
    
    for (const auto& [fluent, polarity] : collector.get_boolean_fluents()) {
        if (polarity == FluentPolarityCollector::Polarity::POSITIVE) {
            analysis.positive_boolean_preconditions.insert(fluent);
        } else {
            analysis.negative_boolean_preconditions.insert(fluent);
        }
    }
    // Store numeric preconditions
    analysis.numeric_preconditions = collector.get_numeric_fluents();
}

void InterferenceAnalysis::analyze_effects(const Action& action, ActionAnalysis& analysis) const {
    for (const Effect& effect : action.effects()) {
        const Expression& fluent = effect.fluent();
        analysis.all_effects.insert(fluent);

        if (effect.is_conditional()) {
            FluentPolarityCollector collector;
            collector.collect_from_expression(effect.condition());
            for (const auto& fluent_in_cond : collector.get_boolean_fluents()) {
                analysis.conditional_effect_fluents.insert(fluent_in_cond.first);
            }
            for (const auto& fluent_in_cond : collector.get_numeric_fluents()) {
                analysis.conditional_effect_fluents.insert(fluent_in_cond);
            }
        }

        // If we have a Boolean assignment ... 
        if (effect.kind() == EffectExpression::Kind::ASSIGN && fluent.type() && fluent.type()->is_bool()) {
            const Expression& value = effect.value();
            if (value.is_atom() && value.value().is_boolean()) {
                if (value.value().boolean()) { // If we're assigning true
                    analysis.positive_boolean_effects.insert(fluent);
                } else {
                    analysis.negative_boolean_effects.insert(fluent);
                }
            }
        } else {
            // This handles all numeric effects: ASSIGN, INCREASE, and DECREASE
            analysis.numeric_effects.insert(fluent);
            
            // Collect dependencies from the RHS of the numeric effect
            FluentPolarityCollector collector;
            collector.collect_from_expression(effect.value());
            for (const auto& dep : collector.get_numeric_fluents()) {
                analysis.numeric_effect_dependencies.insert(dep);
            }
        }
    }
}

bool InterferenceAnalysis::actions_interfere(const Action& a1, const Action& a2) const {
    // Get analysis for both actions
    auto it1 = action_analysis_.find(a1);
    auto it2 = action_analysis_.find(a2);
    
    if (it1 == action_analysis_.end() || it2 == action_analysis_.end()) {
        return false;
    }
    
    const ActionAnalysis& analysis_a1 = it1->second;
    const ActionAnalysis& analysis_a2 = it2->second;
    
    // Helper lambda to check if two sets have non-empty intersection
    auto has_intersection = [](const std::unordered_set<Expression>& set1, 
                              const std::unordered_set<Expression>& set2) -> bool {
        for (const auto& elem : set1) {
            if (set2.find(elem) != set2.end()) {
                return true;
            }
        }
        return false;
    };
    
    // 1. Check if effects of a1 can prevent execution of a2
    // Effects of a1 interfere with preconditions of a2
    if (has_intersection(analysis_a1.positive_boolean_effects, analysis_a2.negative_boolean_preconditions) ||
        has_intersection(analysis_a1.negative_boolean_effects, analysis_a2.positive_boolean_preconditions) ||
        has_intersection(analysis_a1.numeric_effects, analysis_a2.numeric_preconditions)) {
        return true;
    }
    
    // 2. Check if effects of a1 can affect the conditions of a2's conditional effects
    if (has_intersection(analysis_a1.all_effects, analysis_a2.conditional_effect_fluents)) {
        return true;
    }

    // 3. Check if effects of a1 can affect the RHS of a2's numeric effects
    if (has_intersection(analysis_a1.all_effects, analysis_a2.numeric_effect_dependencies)) {
        return true;
    }

    // 4. Check if effects of a1 and a2 interfere (same fluents modified differently or both modified)
    if (has_intersection(analysis_a1.positive_boolean_effects, analysis_a2.negative_boolean_effects) ||
        has_intersection(analysis_a1.negative_boolean_effects, analysis_a2.positive_boolean_effects) ||
        has_intersection(analysis_a1.numeric_effects, analysis_a2.numeric_effects)) {
        return true;
    }
    
    return false;
}

} // namespace planmt