#include "fluent_polarity_collector.hpp"
#include "../problem.hpp"

namespace rantanplan {

void FluentPolarityCollector::collect_fluent_by_id(ExprID eid) {
    if (!eid.valid()) return;

    if (problem_->is_bool_type(eid)) {
        Polarity polarity = in_negation_context_ ? Polarity::NEGATIVE : Polarity::POSITIVE;
        boolean_fluents_[eid] = polarity;
    } else {
        numeric_fluents_.insert(eid);
    }
}

void FluentPolarityCollector::collect_from_id(ExprID eid) {
    if (!eid.valid()) return;
    const ExprPool& pool = problem_->pool();

    if (pool.is_not(eid)) {
        bool old_negation = in_negation_context_;
        in_negation_context_ = !in_negation_context_;

        auto children = pool.children(eid);
        for (size_t i = 1; i < children.size(); ++i) {
            collect_from_id(children[i]);
        }

        in_negation_context_ = old_negation;
        return;
    }

    ExprKind kind = pool.kind(eid);
    if (kind == ExprKind::FLUENT_SYMBOL || kind == ExprKind::STATE_VARIABLE) {
        collect_fluent_by_id(eid);
    } else if (pool.child_count(eid) > 0) {
        for (ExprID child : pool.children(eid)) {
            collect_from_id(child);
        }
    }
}

} // namespace rantanplan
