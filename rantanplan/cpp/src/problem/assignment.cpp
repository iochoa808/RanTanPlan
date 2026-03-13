#include "assignment.hpp"

namespace rantanplan {

std::string Assignment::to_string() const {
    if (pool_) {
        return pool_->to_string(fluent_id_) + " = " + pool_->to_string(value_id_);
    }
    return "eid:" + std::to_string(fluent_id_.id) + " = eid:" + std::to_string(value_id_.id);
}

} // namespace rantanplan
