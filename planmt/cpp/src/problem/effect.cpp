#include "effect.h"

namespace planmt {

Effect::Effect(const pb::Effect& pb_effect) 
    : effect_expr_(pb_effect.effect()) {
}

pb::Effect Effect::to_protobuf() const {
    pb::Effect pb_effect;
    *pb_effect.mutable_effect() = effect_expr_.to_protobuf();
    return pb_effect;
}

} // namespace planmt
