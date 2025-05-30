#include "object.h"

namespace planmt {

Object::Object(const pb::ObjectDeclaration& pb_object) 
    : name_(pb_object.name()), type_(pb_object.type()) {
}

} // namespace planmt
