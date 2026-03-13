#pragma once

#include "protobuf_aliases.hpp"

namespace rantanplan {

class Problem;
class Plan;

/// Deserialize a protobuf Problem into a domain Problem.
Problem load_problem_from_protobuf(const ::Problem& pb_problem);

/// Serialize a domain Plan into a protobuf Plan.
pb::Plan plan_to_protobuf(const Plan& plan);

} // namespace rantanplan
