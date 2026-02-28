#include "interference_analysis.hpp"

namespace rantanplan {


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

    FluentPolarityCollector collector(*problem_);
    collector.collect_from_id(action.precondition_id());

    for (const auto& [eid, polarity] : collector.get_boolean_fluents()) {
        if (polarity == FluentPolarityCollector::Polarity::POSITIVE) {
            analysis.positive_boolean_preconditions.insert(eid);
        } else {
            analysis.negative_boolean_preconditions.insert(eid);
        }
    }
    for (ExprID eid : collector.get_numeric_fluents()) {
        analysis.numeric_preconditions.insert(eid);
    }
}

void InterferenceAnalysis::analyze_effects(const Action& action, ActionAnalysis& analysis) const {
    for (const Effect& effect : action.effects()) {
        const EffectExpression& eff_expr = effect.effect_expression();
        ExprID fluent_eid = eff_expr.fluent_id();
        analysis.all_effects.insert(fluent_eid);

        if (effect.is_conditional()) {
            FluentPolarityCollector collector(*problem_);
            collector.collect_from_id(eff_expr.condition_id());
            for (const auto& [eid, _] : collector.get_boolean_fluents()) {
                analysis.conditional_effect_fluents.insert(eid);
            }
            for (ExprID eid : collector.get_numeric_fluents()) {
                analysis.conditional_effect_fluents.insert(eid);
            }
        }

        if (effect.kind() == EffectExpression::Kind::ASSIGN && problem_->is_bool_type(fluent_eid)) {
            ExprID value_eid = eff_expr.value_id();
            const ExprPool& pool = problem_->pool();
            if (pool.is_constant(value_eid) && pool.payload_is_bool(value_eid)) {
                if (pool.payload_bool(value_eid)) {
                    analysis.positive_boolean_effects.insert(fluent_eid);
                } else {
                    analysis.negative_boolean_effects.insert(fluent_eid);
                }
            }
        } else {
            analysis.numeric_effects.insert(fluent_eid);

            FluentPolarityCollector collector(*problem_);
            collector.collect_from_id(eff_expr.value_id());
            for (ExprID dep : collector.get_numeric_fluents()) {
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
    auto has_intersection = [](const std::unordered_set<ExprID>& set1,
                              const std::unordered_set<ExprID>& set2) -> bool {
        for (const auto& elem : set1) {
            if (set2.find(elem) != set2.end()) {
                return true;
            }
        }
        return false;
    };

    // 1. Check if effects of a1 can prevent execution of a2
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

} // namespace rantanplan
