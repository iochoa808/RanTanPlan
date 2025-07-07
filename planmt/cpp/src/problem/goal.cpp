#include "goal.h"

namespace planmt {

Goal::Goal(const pb::Goal& pb_goal, const Problem* problem) 
    : goal_expr_(pb_goal.goal(), problem) {
    // Note: We ignore timing information and weights for non-temporal planning
}

} // namespace planmt
