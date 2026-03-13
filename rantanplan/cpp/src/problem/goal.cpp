#include "goal.hpp"

namespace rantanplan {

std::string Goal::to_string() const {
    if (pool_) return pool_->to_string(goal_id_);
    return "eid:" + std::to_string(goal_id_.id);
}

} // namespace rantanplan
