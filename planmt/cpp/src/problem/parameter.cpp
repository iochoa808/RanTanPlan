#include "parameter.h"

namespace planmt {

Parameter::Parameter(const pb::Parameter& pb_parameter) 
    : name_(pb_parameter.name()), type_(pb_parameter.type()) {
}

pb::Parameter Parameter::to_protobuf() const {
    pb::Parameter pb_parameter;
    pb_parameter.set_name(name_);
    pb_parameter.set_type(type_);
    return pb_parameter;
}

} // namespace planmt
