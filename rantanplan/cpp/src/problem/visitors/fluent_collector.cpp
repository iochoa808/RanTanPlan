#include "fluent_collector.hpp"
#include "../problem.hpp"

namespace rantanplan {

void FluentCollector::collect_fluent_by_id(ExprID eid) {
    if (!eid.valid()) return;
    fluents_.insert(eid);
}

void FluentCollector::collect_from_id(ExprID eid) {
    if (!eid.valid()) return;
    const ExprPool& pool = problem_->pool();

    ExprKind kind = pool.kind(eid);
    if (kind == ExprKind::STATE_VARIABLE) {
        collect_fluent_by_id(eid);
    }

    if (pool.child_count(eid) > 0) {
        for (ExprID child : pool.children(eid)) {
            collect_from_id(child);
        }
    }
}

} // namespace rantanplan
