#include "parameter.h"

namespace planmt {

Parameter::Parameter(const pb::Parameter& pb_parameter) 
    : name_(pb_parameter.name()), type_(pb_parameter.type()) {
}

} // namespace planmt
