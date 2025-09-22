#include "assignment.hpp"

namespace planmt {

Assignment::Assignment(const pb::Assignment& pb_assignment, const Problem* problem) 
    : fluent_(pb_assignment.fluent(), problem), value_(pb_assignment.value(), problem) {
}

std::string Assignment::to_string() const {
    return fluent_.to_string() + " := " + value_.to_string();
}

} // namespace planmt
