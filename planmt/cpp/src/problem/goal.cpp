#include "goal.h"

namespace planmt {

Goal::Goal(const pb::Goal& pb_goal) 
    : goal_expr_(pb_goal.goal()) {
    // Note: We ignore timing information and weights for non-temporal planning
}

} // namespace planmt
