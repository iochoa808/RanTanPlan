#include "assignment.h"

namespace planmt {

Assignment::Assignment(const pb::Assignment& pb_assignment) 
    : fluent_(pb_assignment.fluent()), value_(pb_assignment.value()) {
}

std::string Assignment::to_string() const {
    return fluent_.to_string() + " := " + value_.to_string();
}

} // namespace planmt
