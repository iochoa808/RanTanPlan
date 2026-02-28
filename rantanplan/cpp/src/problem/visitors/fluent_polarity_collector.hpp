#pragma once

#include "../expr_pool.hpp"
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vector>

namespace rantanplan {

// Forward declaration
class Problem;

/**
 * @brief Collects fluents with their polarity information as ExprIDs
 *
 * Walks the ExprPool tree and collects all fluent applications
 * with their polarity (positive/negative) in boolean contexts.
 * For numeric fluents, polarity is not relevant.
 */
class FluentPolarityCollector {
public:
    enum class Polarity {
        POSITIVE,
        NEGATIVE
    };

private:
    const Problem* problem_ = nullptr;
    std::unordered_map<ExprID, Polarity> boolean_fluents_;
    std::unordered_set<ExprID> numeric_fluents_;
    bool in_negation_context_ = false;

    void collect_fluent_by_id(ExprID eid);

public:
    explicit FluentPolarityCollector(const Problem& problem) : problem_(&problem) {}

    void collect_from_id(ExprID eid);

    const std::unordered_map<ExprID, Polarity>& get_boolean_fluents() const {
        return boolean_fluents_;
    }

    const std::unordered_set<ExprID>& get_numeric_fluents() const {
        return numeric_fluents_;
    }

    bool has_boolean_fluents() const { return !boolean_fluents_.empty(); }
    bool has_numeric_fluents() const { return !numeric_fluents_.empty(); }
    size_t boolean_fluent_count() const { return boolean_fluents_.size(); }
    size_t numeric_fluent_count() const { return numeric_fluents_.size(); }

    void clear() {
        boolean_fluents_.clear();
        numeric_fluents_.clear();
        in_negation_context_ = false;
    }
};

} // namespace rantanplan
