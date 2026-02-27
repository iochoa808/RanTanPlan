#pragma once

#include "../expr_pool.hpp"
#include <unordered_set>
#include <string>
#include <vector>

namespace rantanplan {

// Forward declaration
class Problem;

/**
 * @brief Collects all fluents in an expression as ExprIDs
 *
 * Walks the ExprPool tree and collects all fluent applications
 * (state variables) that appear in the expression.
 */
class FluentCollector {
private:
    const Problem* problem_ = nullptr;
    std::unordered_set<ExprID> fluents_;

    void collect_fluent_by_id(ExprID eid);

public:
    explicit FluentCollector(const Problem& problem) : problem_(&problem) {}

    void collect_from_id(ExprID eid);

    const std::unordered_set<ExprID>& get_fluents() const { return fluents_; }

    bool has_fluents() const { return !fluents_.empty(); }
    size_t fluent_count() const { return fluents_.size(); }
    void clear() { fluents_.clear(); }
};

} // namespace rantanplan
