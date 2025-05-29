#include "assignment.h"

namespace planmt {

Assignment::Assignment(const pb::Assignment& pb_assignment) 
    : fluent_(pb_assignment.fluent()), value_(pb_assignment.value()) {
}

std::string Assignment::to_string() const {
    return fluent_.to_string() + " := " + value_.to_string();
}

pb::Assignment Assignment::to_protobuf() const {
    pb::Assignment pb_assignment;
    *pb_assignment.mutable_fluent() = fluent_.to_protobuf();
    *pb_assignment.mutable_value() = value_.to_protobuf();
    return pb_assignment;
}

} // namespace planmt
