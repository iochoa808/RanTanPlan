#include "goal.hpp"

namespace planmt {

Goal::Goal(const pb::Goal& pb_goal, const Problem* problem) 
    : goal_expr_(pb_goal.goal(), problem) {
}

} // namespace planmt
