#include "effect.h"

namespace planmt {

Effect::Effect(const pb::Effect& pb_effect) 
    : effect_expr_(pb_effect.effect()) {
}

} // namespace planmt
