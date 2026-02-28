#include "goal.hpp"
#include "problem.hpp"

namespace rantanplan {

Goal::Goal(const pb::Goal& pb_goal, Problem* problem) {
    pool_ = &problem->pool();
    goal_id_ = problem->intern_from_protobuf(pb_goal.goal());
}

std::string Goal::to_string() const {
    if (pool_) return pool_->to_string(goal_id_);
    return "eid:" + std::to_string(goal_id_.id);
}

} // namespace rantanplan
