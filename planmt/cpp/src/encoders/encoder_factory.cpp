#include "encoder_factory.h"
#include <stdexcept>

namespace planmt {

std::unique_ptr<BaseEncoder> EncoderFactory::create_encoder(EncoderType type, const Problem& problem, z3::context& ctx) {
    switch (type) {
        case EncoderType::GROUNDED:
            return std::make_unique<GroundedEncoder>(problem, ctx);
        case EncoderType::REIFIED:
            return std::make_unique<ReifiedGroundedEncoder>(problem, ctx);
        default:
            throw std::invalid_argument("Unknown encoder type");
    }
}

std::unique_ptr<BaseEncoder> EncoderFactory::create_encoder(const std::string& type_name, const Problem& problem, z3::context& ctx) {
    return create_encoder(string_to_type(type_name), problem, ctx);
}

EncoderFactory::EncoderType EncoderFactory::string_to_type(const std::string& type_name) {
    if (type_name == "grounded") {
        return EncoderType::GROUNDED;
    } else if (type_name == "reified") {
        return EncoderType::REIFIED;
    } else {
        throw std::invalid_argument("Unknown encoder type: " + type_name);
    }
}

} // namespace planmt
