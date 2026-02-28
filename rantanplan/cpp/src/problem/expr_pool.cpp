#include "expr_pool.hpp"
#include <sstream>

namespace rantanplan {

std::string ExprNode::canonical_key() const {
    // Format: "kind:op:type_id|payload_tag:payload_val|c0,c1,..."
    std::string key;
    key += std::to_string(kind);
    key += ':';
    key += std::to_string(op);
    key += ':';
    key += std::to_string(type_id);
    key += '|';

    std::visit([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            key += "n:";
        } else if constexpr (std::is_same_v<T, bool>) {
            key += "b:";
            key += val ? '1' : '0';
        } else if constexpr (std::is_same_v<T, int64_t>) {
            key += "i:";
            key += std::to_string(val);
        } else if constexpr (std::is_same_v<T, double>) {
            key += "d:";
            key += std::to_string(val);
        } else if constexpr (std::is_same_v<T, std::string>) {
            key += "s:";
            key += std::to_string(val.size());
            key += ':';
            key += val;
        }
    }, payload);

    key += '|';
    for (size_t i = 0; i < children.size(); ++i) {
        if (i > 0) key += ',';
        key += std::to_string(children[i].id);
    }
    return key;
}

ExprID ExprPool::intern(ExprNode node) {
    std::string key = node.canonical_key();
    auto it = intern_.find(key);
    if (it != intern_.end()) {
        return it->second;
    }
    ExprID new_id{static_cast<int32_t>(nodes_.size())};
    nodes_.push_back(std::move(node));
    intern_[std::move(key)] = new_id;
    return new_id;
}

std::string ExprPool::to_string(ExprID id) const {
    if (!id.valid()) return "<null>";

    const ExprNode& node = get(id);

    // Leaf: payload is the display value
    if (node.children.empty()) {
        return std::visit([](auto&& val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "()";
            } else if constexpr (std::is_same_v<T, bool>) {
                return val ? "true" : "false";
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return val;
            }
        }, node.payload);
    }

    // Compound: (child0 child1 ...)
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < node.children.size(); ++i) {
        if (i > 0) oss << " ";
        oss << to_string(node.children[i]);
    }
    oss << ")";
    return oss.str();
}

} // namespace rantanplan
