#pragma once

#include <string>
#include <memory>
#include <cstdint>

namespace rantanplan {

class Type {
public:
    // [XTS] Structural kind of a type, classified ONCE from its name
    enum class Kind : uint8_t { Object, Bool, Int, BoundedInt, Real, Array, Set };

    Type() = default;
    explicit Type(const std::string& name);

    const std::string& name() const;
    void set_name(const std::string& name);

    Kind kind() const { return kind_; }

    // Parent type for hierarchy (nullptr if root)
    const Type* parent() const;
    void set_parent(const Type* parent);

    // For initial parsing, store parent name until resolved
    void set_parent_name(const std::string& parent_name);
    const std::string& parent_name() const;

    // Subtype check (traverse up the hierarchy)
    bool is_subtype_of(const Type* supertype) const;

    // Primitive type helpers. From cached kind_, no string parsing.
    bool is_bool() const { return kind_ == Kind::Bool; }
    // [XTS-MOD] also true for bounded ints
    bool is_int() const { return kind_ == Kind::Int || kind_ == Kind::BoundedInt; }
    bool is_real() const { return kind_ == Kind::Real; }

    // [XTS] Set type and its elements
    bool is_set() const;
    std::string set_element_type_name() const;

    // [XTS] Array type and its elements
    bool is_array() const;
    int64_t array_size() const;
    std::string array_element_type_name() const;

    bool is_object() const { return kind_ == Kind::Object && !is_array() && !is_set(); }

    // [XTS] Bounded integer and its bounds
    bool is_bounded_int() const { return kind_ == Kind::BoundedInt; }
    int64_t lower_bound() const { return lower_bound_; }
    int64_t upper_bound() const { return upper_bound_; }

    // [XTS] Walk the type hierarchy to find the tightest declared bounds.
    // Returns the first Type in the chain (self or ancestor) that is_bounded_int(), or nullptr if not found
    const Type* bounded_int_ancestor() const;

    // [XTS] Set element type and array size from proto TypeDeclaration structured fields.
    void set_element_info(const std::string& element_type_name, int64_t size = -1);

private:
    // Classify name into kind_ (and bounds for BoundedInt)
    void classify_from_name(const std::string& name);

    std::string name_;
    const Type* parent_ = nullptr;
    std::string parent_name_;

    Kind    kind_        = Kind::Object; // [XTS] classified once from name_
    int64_t lower_bound_ = 0;            // [XTS] valid iff kind_ == BoundedInt
    int64_t upper_bound_ = 0;            // [XTS] valid iff kind_ == BoundedInt

    // [XTS] Populated from proto TypeDeclaration.element_type / .size when available.
    // Empty / -1 means the accessor falls back to string parsing.
    std::string element_type_name_;
    int64_t     stored_array_size_ = -1;
};

} // namespace rantanplan
