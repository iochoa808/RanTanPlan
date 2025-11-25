#include "object.hpp"

namespace rantanplan {

Object::Object(const pb::ObjectDeclaration& pb_object, const Type* type)
    : name_(pb_object.name()), type_(type) {}

} // namespace rantanplan
