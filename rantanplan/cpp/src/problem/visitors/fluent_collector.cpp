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
        // Only collect fully-grounded fluents: all arguments must be concrete
        // (CONSTANT or FLUENT_SYMBOL) not VARIABLE, PARAMETER, or another
        // STATE_VARIABLE. Non-constant arguments come from a quatifier precondition not expanded
        bool is_grounded = true;
        for (ExprID child : pool.children(eid)) {
            if (child.valid()) {
                ExprKind ck = pool.kind(child);
                if (ck == ExprKind::STATE_VARIABLE ||
                    ck == ExprKind::VARIABLE ||
                    ck == ExprKind::PARAMETER) {
                    is_grounded = false;
                    break;
                }
            }
        }
        if (is_grounded) {
            collect_fluent_by_id(eid);
        }
    }

    if (pool.child_count(eid) > 0) {
        for (ExprID child : pool.children(eid)) {
            collect_from_id(child);
        }
    }
}

} // namespace rantanplan
