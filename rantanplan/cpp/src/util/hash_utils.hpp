#pragma once

#include <cstddef>
#include <functional>
#include <utility>

namespace rantanplan {

/// Hash functor for std::pair<int,int>, suitable for use with unordered containers.
struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const {
        auto h1 = std::hash<int>{}(p.first);
        auto h2 = std::hash<int>{}(p.second);
        return h1 ^ (h2 * 2654435761u);
    }
};

} // namespace rantanplan
