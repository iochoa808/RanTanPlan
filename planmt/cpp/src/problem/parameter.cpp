#include "parameter.hpp"

namespace planmt {

Parameter::Parameter(const pb::Parameter& pb_param, const Type* type)
    : name_(pb_param.name()), type_(type) {}

} // namespace planmt
