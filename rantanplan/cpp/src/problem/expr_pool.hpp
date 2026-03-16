#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cassert>
#include <span>
#include "expr_enums.hpp"

// Optional Improvements
// ---------------------
// - Replace `ExprPool::canonical_key()` string hashing with structural hash
//   (hash `kind`, `op`, `type_id`, children IDs, payload directly into `size_t`).
//   Currently string-based — only affects construction time, not runtime.
// - Add `ExprPool::freeze()` to drop the `intern_` map after construction, freeing memory.

namespace rantanplan {

/// Lightweight handle to an interned expression. O(1) equality and hashing.
struct ExprID {
    int32_t id = -1;

    bool valid() const { return id >= 0; }
    bool operator==(ExprID other) const { return id == other.id; }
    bool operator!=(ExprID other) const { return id != other.id; }
    bool operator<(ExprID other) const { return id < other.id; }
};

inline constexpr ExprID EXPR_NULL{-1};

} // namespace rantanplan

// Hash specialisation — delegates to the underlying int32_t hash
namespace std {
template <>
struct hash<rantanplan::ExprID> : hash<int32_t> {
    size_t operator()(rantanplan::ExprID eid) const noexcept {
        return hash<int32_t>::operator()(eid.id);
    }
};
} // namespace std

namespace rantanplan {

/// Payload carried by leaf nodes in the expression pool.
using ExprPayload = std::variant<std::monostate, bool, int64_t, double, std::string>;

/// A single node in the expression pool (internal representation).
struct ExprNode {
    int kind = 0;     // ExprKind as int
    int op = -1;      // ExprOperator as int, -1 if N/A
    int type_id = -1; // index into Problem's types_ vector, -1 if null

    std::vector<ExprID> children;
    ExprPayload payload; // leaf value (string for symbols, int64 for ints, etc.)

    /// Build a canonical string key for interning (used as hash map key).
    std::string canonical_key() const;
};

/// Append-only, structurally-interning expression pool.
///
/// Every expression is stored exactly once. Structurally identical expressions
/// share the same ExprID, making equality comparison O(1).
///
/// The pool is designed to be shared (via shared_ptr) across immutable
/// Problem instances in a transformation pipeline.
class ExprPool {
public:
    // ------------------------------------------------------------------------
    // Expression child-layout conventions
    //
    // Most expression kinds expose their full child list directly via
    // children()/child()/child_count().
    //
    // Two kinds use a "head+arguments" layout:
    //   - ExprKind::FUNCTION_APPLICATION
    //   - ExprKind::STATE_VARIABLE
    //
    // For these, children[0] is the head symbol (operator/fluent symbol), and
    // children[1..] are semantic arguments.
    //
    // Use the application helpers when you mean semantic arguments/head:
    //   - has_head_and_arguments(id)
    //   - argument_count(id), argument(id, i), arguments(id)
    //   - head_symbol_id(id), head_symbol_node(id)
    //
    // Use raw child access when you need the exact structural tree layout
    // including the head symbol (e.g., generic traversals, serialization,
    // structural formatting/debug output).
    // ------------------------------------------------------------------------
    ExprPool() = default;

    /// Intern a fully-constructed ExprNode. Returns existing ID if structurally
    /// identical node already exists, otherwise appends and returns new ID.
    ExprID intern(ExprNode node);

    /// Access a node by ID.
    const ExprNode& get(ExprID id) const {
        assert(id.valid() && static_cast<size_t>(id.id) < nodes_.size());
        return nodes_[id.id];
    }

    /// Number of interned nodes.
    size_t size() const { return nodes_.size(); }

    /// Build a human-readable string for an expression (for debugging/display).
    std::string to_string(ExprID id) const;

    // ========================================================================
    // Kind / Operator / Type accessors
    // ========================================================================

    ExprKind kind(ExprID id) const { return static_cast<ExprKind>(get(id).kind); }
    ExprOperator op(ExprID id) const { return static_cast<ExprOperator>(get(id).op); }
    int type_id(ExprID id) const { return get(id).type_id; }

    // ========================================================================
    // Structural checks (based on kind)
    // ========================================================================

    bool is_leaf(ExprID id) const { return get(id).children.empty(); }
    bool is_constant(ExprID id) const { return kind(id) == ExprKind::CONSTANT; }
    bool is_parameter(ExprID id) const { return kind(id) == ExprKind::PARAMETER; }
    bool is_variable(ExprID id) const { return kind(id) == ExprKind::VARIABLE; }
    bool is_fluent_symbol(ExprID id) const { return kind(id) == ExprKind::FLUENT_SYMBOL; }
    bool is_function_symbol(ExprID id) const { return kind(id) == ExprKind::FUNCTION_SYMBOL; }
    bool is_state_variable(ExprID id) const { return kind(id) == ExprKind::STATE_VARIABLE; }
    bool is_function_application(ExprID id) const { return kind(id) == ExprKind::FUNCTION_APPLICATION; }

    // ========================================================================
    // Operator checks (based on op field)
    // ========================================================================

    bool is_and(ExprID id) const { return op(id) == ExprOperator::AND; }
    bool is_or(ExprID id) const { return op(id) == ExprOperator::OR; }
    bool is_not(ExprID id) const { return op(id) == ExprOperator::NOT; }
    bool is_implies(ExprID id) const { return op(id) == ExprOperator::IMPLIES; }
    bool is_iff(ExprID id) const { return op(id) == ExprOperator::IFF; }
    bool is_equals(ExprID id) const { return op(id) == ExprOperator::EQUALS; }
    bool is_plus(ExprID id) const { return op(id) == ExprOperator::PLUS; }
    bool is_minus(ExprID id) const { return op(id) == ExprOperator::MINUS; }
    bool is_multiply(ExprID id) const { return op(id) == ExprOperator::MULTIPLY; }
    bool is_divide(ExprID id) const { return op(id) == ExprOperator::DIVIDE; }
    bool is_less_than(ExprID id) const { return op(id) == ExprOperator::LESS_THAN; }
    bool is_less_equal(ExprID id) const { return op(id) == ExprOperator::LESS_EQUAL; }
    bool is_greater_than(ExprID id) const { return op(id) == ExprOperator::GREATER_THAN; }
    bool is_greater_equal(ExprID id) const { return op(id) == ExprOperator::GREATER_EQUAL; }

    // ========================================================================
    // Children access
    // ========================================================================

    const std::vector<ExprID>& children(ExprID id) const { return get(id).children; }
    size_t child_count(ExprID id) const { return get(id).children.size(); }
    ExprID child(ExprID id, size_t index) const {
        const auto& c = get(id).children;
        assert(index < c.size());
        return c[index];
    }

    // ========================================================================
    // Head+arguments accessors
    //
    // In this codebase, both FUNCTION_APPLICATION and STATE_VARIABLE encode a
    // head symbol at children[0] and arguments at children[1..].
    // ========================================================================

    bool has_head_and_arguments(ExprID id) const {
        ExprKind k = kind(id);
        return k == ExprKind::FUNCTION_APPLICATION || k == ExprKind::STATE_VARIABLE;
    }

    size_t argument_count(ExprID id) const {
        assert(has_head_and_arguments(id));
        size_t n = child_count(id);
        assert(n > 0);
        return n - 1;
    }

    ExprID argument(ExprID id, size_t index) const {
        assert(index < argument_count(id));
        return child(id, index + 1);
    }

    std::span<const ExprID> arguments(ExprID id) const {
        assert(has_head_and_arguments(id));
        const auto& kids = children(id);
        assert(!kids.empty());
        return std::span<const ExprID>(kids.data() + 1, kids.size() - 1);
    }

    ExprID head_symbol_id(ExprID id) const {
        assert(has_head_and_arguments(id));
        assert(child_count(id) > 0);
        return child(id, 0);
    }

    const ExprNode& head_symbol_node(ExprID id) const {
        return get(head_symbol_id(id));
    }

    // ========================================================================
    // Payload access
    // ========================================================================

    const ExprPayload& payload(ExprID id) const { return get(id).payload; }

    bool payload_is_bool(ExprID id) const {
        return std::holds_alternative<bool>(get(id).payload);
    }
    bool payload_bool(ExprID id) const {
        return std::get<bool>(get(id).payload);
    }
    bool payload_is_int(ExprID id) const {
        return std::holds_alternative<int64_t>(get(id).payload);
    }
    int64_t payload_int(ExprID id) const {
        return std::get<int64_t>(get(id).payload);
    }
    bool payload_is_double(ExprID id) const {
        return std::holds_alternative<double>(get(id).payload);
    }
    double payload_double(ExprID id) const {
        return std::get<double>(get(id).payload);
    }
    bool payload_is_string(ExprID id) const {
        return std::holds_alternative<std::string>(get(id).payload);
    }
    const std::string& payload_string(ExprID id) const {
        return std::get<std::string>(get(id).payload);
    }

    // ========================================================================
    // Semantic helpers
    // ========================================================================

    /// Intern an integer constant and return its ExprID.
    ExprID intern_int_constant(int64_t value) {
        ExprNode node;
        node.kind = static_cast<int>(ExprKind::CONSTANT);
        node.payload = value;
        return intern(std::move(node));
    }

    /// Check if this is a boolean constant with value true.
    bool is_true_constant(ExprID id) const {
        if (!id.valid()) return false;
        const auto& n = get(id);
        return n.children.empty() &&
               static_cast<ExprKind>(n.kind) == ExprKind::CONSTANT &&
               std::holds_alternative<bool>(n.payload) &&
               std::get<bool>(n.payload) == true;
    }

    /// Check if this is a boolean constant with value false.
    bool is_false_constant(ExprID id) const {
        if (!id.valid()) return false;
        const auto& n = get(id);
        return n.children.empty() &&
               static_cast<ExprKind>(n.kind) == ExprKind::CONSTANT &&
               std::holds_alternative<bool>(n.payload) &&
               std::get<bool>(n.payload) == false;
    }

private:
    std::vector<ExprNode> nodes_;
    std::unordered_map<std::string, ExprID> intern_;
};

} // namespace rantanplan
