#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rantanplan {

// [XTS] IPAR cell SV name parser
//
// IPAR (the UP int-parameter-actions remover) grounds subscript notation on
// ArrayType fluents into arity-0 scalar SVs named "base[i0][i1]...".
// Returns {base_name, {i0, i1, ...}}, or nullopt if the name doesn't follow
// the pattern (no bracket group, unclosed bracket, non-numeric index, or
// trailing characters after the last ']').
//
// Pre/Post:
//   "board[1][2]"      ->  {"board",   {1, 2}}
//   "card_at[-1][0]"   ->  {"card_at", {-1, 0}} (out-of-bounds sentinel)
//   "fuel"             ->  nullopt              (no brackets)
//   "[0]"              ->  nullopt              (empty base name)

inline std::optional<std::pair<std::string, std::vector<int64_t>>>
parse_ipar_cell_name(const std::string& name) {
    // The base name is everything before the first '['. Reject "no brackets
    // at all" (bracket == npos) and "empty base name" (bracket == 0, e.g. "[0]").
    size_t bracket = name.find('[');
    if (bracket == std::string::npos || bracket == 0) return std::nullopt;
    // Walk consecutive "[<int>]" groups starting right after the base name,
    std::vector<int64_t> idxs;
    size_t pos = bracket;
    while (pos < name.size() && name[pos] == '[') {
        size_t close = name.find(']', pos + 1);
        if (close == std::string::npos) return std::nullopt;  // unclosed bracket
        // Parse the text between the brackets as an integer index
        try {
            idxs.push_back(std::stoll(name.substr(pos + 1, close - pos - 1)));
        } catch (...) { return std::nullopt; }

        pos = close + 1;  // resume right after ']'
    }

    // A valid cell name has at least one index group and nothing left over
    // after the last ']'
    if (idxs.empty() || pos != name.size()) return std::nullopt;

    return std::make_pair(name.substr(0, bracket), std::move(idxs));
}

// Inverse of parse_ipar_cell_name: build the cell SV name for a base fluent and
// an outermost-first index list.  make_ipar_cell_name("board", {1, 2}) → "board[1][2]".

//
// Pre/Post:
//   {"board",   {1, 2}}    ->  "board[1][2]"
//   {"card_at", {-1, 0}}   ->  (out-of-bounds sentinel)

inline std::string make_ipar_cell_name(const std::string& base,
                                       const std::vector<int64_t>& idxs) {
    std::string name = base;
    for (int64_t k : idxs) {
        name += "[" + std::to_string(k) + "]";
    }
    return name;
}

}  // namespace rantanplan
