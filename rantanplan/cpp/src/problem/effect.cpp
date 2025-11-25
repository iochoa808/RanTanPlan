#include "effect.hpp"

namespace rantanplan {

Effect::Effect(const pb::Effect& pb_effect, const Problem* problem) 
    : effect_expr_(pb_effect.effect(), problem) {
}

} // namespace rantanplan
