#include "goal.h"

namespace planmt {

Goal::Goal(const pb::Goal& pb_goal) 
    : goal_expr_(pb_goal.goal()) {
    // Note: We ignore timing information and weights for non-temporal planning
}

pb::Goal Goal::to_protobuf() const {
    pb::Goal pb_goal;
    *pb_goal.mutable_goal() = goal_expr_.to_protobuf();
    // Note: We don't set timing or weight information for non-temporal planning
    return pb_goal;
}

} // namespace planmt
