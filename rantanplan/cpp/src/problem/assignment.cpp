#include "assignment.hpp"
#include "problem.hpp"

namespace rantanplan {

Assignment::Assignment(const pb::Assignment& pb_assignment, Problem* problem) {
    pool_ = &problem->pool();
    fluent_id_ = problem->intern_from_protobuf(pb_assignment.fluent());
    value_id_ = problem->intern_from_protobuf(pb_assignment.value());
}

std::string Assignment::to_string() const {
    if (pool_) {
        return pool_->to_string(fluent_id_) + " = " + pool_->to_string(value_id_);
    }
    return "eid:" + std::to_string(fluent_id_.id) + " = eid:" + std::to_string(value_id_.id);
}

} // namespace rantanplan
