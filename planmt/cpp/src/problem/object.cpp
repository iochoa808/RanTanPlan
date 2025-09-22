#include "object.hpp"

namespace planmt {

Object::Object(const pb::ObjectDeclaration& pb_object, const Type* type)
    : name_(pb_object.name()), type_(type) {}

} // namespace planmt
