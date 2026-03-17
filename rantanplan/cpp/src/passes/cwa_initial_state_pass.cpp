#include "cwa_initial_state_pass.hpp"
#include "../util/logger.hpp"
#include "../util/scoped_timer.hpp"
#include <unordered_set>

namespace rantanplan {

void CWAInitialStatePass::apply(PipelineResult& result) const {
    ScopedTimer timer("cwa.time_ms");

    auto& problem = result.problem;
    auto pool = problem.pool_ptr();

    // Build set of fluent ExprIDs that already have an initial assignment.
    std::unordered_set<ExprID> assigned;
    for (const auto& assignment : problem.initial_state()) {
        assigned.insert(assignment.fluent_id());
    }

    // Find grounded fluents missing from the initial state.
    std::vector<Assignment> defaults;
    for (ExprID eid : problem.grounded_fluents()) {
        if (assigned.count(eid)) continue;

        // Determine the fluent's type and create the appropriate default constant.
        const ExprNode& node = pool->get(eid);
        int fluent_type_id = node.type_id;
        if (fluent_type_id < 0) continue;  // unknown type — skip

        const Type* type = problem.type_for_id(eid);
        if (!type) continue;

        ExprNode constant_node;
        constant_node.kind = static_cast<int>(ExprKind::CONSTANT);
        constant_node.type_id = fluent_type_id;

        if (type->is_bool()) {
            constant_node.payload = false;
        } else if (type->is_int()) {
            constant_node.payload = static_cast<int64_t>(0);
        } else if (type->is_real()) {
            constant_node.payload = 0.0;
        } else {
            continue;  // object-typed fluent — no sensible default
        }

        ExprID value_id = pool->intern(std::move(constant_node));
        defaults.emplace_back(eid, value_id, &problem.pool());
    }

    if (defaults.empty()) {
        Logger::instance().component(VerbosityLevel::VERBOSE, "CWA", {
            {"time", std::to_string(static_cast<int>(timer.elapsed_ms())) + "ms"},
            {"status", "all grounded fluents already assigned"}
        });
        return;
    }

    Logger::instance().component(VerbosityLevel::INFO, "CWA", {
        {"time", std::to_string(static_cast<int>(timer.elapsed_ms())) + "ms"},
        {"added", std::to_string(defaults.size()) + " default assignments"}
    });

    result.problem = problem.with_additional_initial_state(defaults);
}

} // namespace rantanplan
