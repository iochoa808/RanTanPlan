#include "object.h"

namespace planmt {

Object::Object(const pb::ObjectDeclaration& pb_object) 
    : name_(pb_object.name()), type_(pb_object.type()) {
}

pb::ObjectDeclaration Object::to_protobuf() const {
    pb::ObjectDeclaration pb_object;
    pb_object.set_name(name_);
    pb_object.set_type(type_);
    return pb_object;
}

} // namespace planmt
