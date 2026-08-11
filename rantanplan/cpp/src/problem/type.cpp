#include "type.hpp"
#include <stdexcept>

namespace rantanplan {

// Small helper: does `name` match `prefix`...`close` (e.g. "up:array[" ... ']')?
static bool has_wrapped_prefix(const std::string& name, const std::string& prefix, char close) {
    return name.size() > prefix.size() &&
           name.compare(0, prefix.size(), prefix) == 0 &&
           name.back() == close;
}

// [XTS] Classify the type's structural kind from its name, once
void Type::classify_from_name(const std::string& name) {
    kind_ = Kind::Object;
    lower_bound_ = 0;
    upper_bound_ = 0;

    if (name == "bool" || name == "up:bool") {
        kind_ = Kind::Bool;
    } else if (name == "int" || name == "up:int" ||
               name == "integer" || name == "up:integer") {
        kind_ = Kind::Int;
    } else if (name == "real" || name == "up:real") {
        kind_ = Kind::Real;
    } else if (has_wrapped_prefix(name, "up:array[", ']')) {
        kind_ = Kind::Array;
    } else if (has_wrapped_prefix(name, "up:set{", '}')) {
        kind_ = Kind::Set;
    } else if (has_wrapped_prefix(name, "up:integer[", ']')) {
        static const std::string kPrefix = "up:integer[";
        const std::string inner = name.substr(kPrefix.size(),
                                               name.size() - kPrefix.size() - 1);
        const auto comma = inner.find(',');
        if (comma != std::string::npos) {
            try {
                lower_bound_ = std::stoll(inner.substr(0, comma));
                size_t rstart = comma + 1;
                while (rstart < inner.size() && inner[rstart] == ' ') ++rstart;
                upper_bound_ = std::stoll(inner.substr(rstart));
                kind_ = Kind::BoundedInt;
            } catch (...) {
                // Malformed string — stay Kind::Object.
                lower_bound_ = 0;
                upper_bound_ = 0;
            }
        }
    }
}

Type::Type(const std::string& name) : name_(name) {
    classify_from_name(name);
}

void Type::set_element_info(const std::string& element_type_name, int64_t size) { // [XTS]
    element_type_name_ = element_type_name;
    // Proto default 0 means "field not set"
    if (size > 0) stored_array_size_ = size;
}

const std::string& Type::name() const { return name_; }
void Type::set_name(const std::string& name) {
    name_ = name;
    classify_from_name(name);
}

const Type* Type::parent() const { return parent_; }
void Type::set_parent(const Type* parent) { parent_ = parent; }

void Type::set_parent_name(const std::string& parent_name) { parent_name_ = parent_name; }
const std::string& Type::parent_name() const { return parent_name_; }

bool Type::is_subtype_of(const Type* supertype) const {
    const Type* current = this;
    while (current) {
        if (current == supertype) return true;
        current = current->parent();
    }
    return false;
}

bool Type::is_set() const {
    for (const Type* t = this; t; t = t->parent()) {
        if (t->kind_ == Kind::Set) return true;
    }
    return false;
}

std::string Type::set_element_type_name() const { // [XTS]
    static const std::string kPrefix = "up:set{";
    for (const Type* t = this; t; t = t->parent()) {
        if (t->kind_ != Kind::Set) continue;
        if (!t->element_type_name_.empty()) return t->element_type_name_;
        return t->name_.substr(kPrefix.size(), t->name_.size() - kPrefix.size() - 1);
    }
    return "";
}

bool Type::is_array() const { // [XTS] cached-kind ancestor walk, no string parsing
    for (const Type* t = this; t; t = t->parent()) {
        if (t->kind_ == Kind::Array) return true;
    }
    return false;
}

// [XTS] Parse "up:array[N,T]"
static int64_t parse_array_size(const std::string& name) {
    static const std::string kPrefix = "up:array[";
    if (name.size() <= kPrefix.size() || name.compare(0, kPrefix.size(), kPrefix) != 0 || name.back() != ']')
        return -1;
    const std::string inner = name.substr(kPrefix.size(), name.size() - kPrefix.size() - 1);
    int depth = 0;
    for (size_t i = 0; i < inner.size(); ++i) {
        if (inner[i] == '[') ++depth;
        else if (inner[i] == ']') --depth;
        else if (inner[i] == ',' && depth == 0) {
            try { return std::stoll(inner.substr(0, i)); } catch (...) { return -1; }
        }
    }
    return -1;
}

static std::string parse_array_elem_type(const std::string& name) { // [XTS]
    static const std::string kPrefix = "up:array[";
    if (name.size() <= kPrefix.size() || name.compare(0, kPrefix.size(), kPrefix) != 0 || name.back() != ']')
        return "";
    const std::string inner = name.substr(kPrefix.size(), name.size() - kPrefix.size() - 1);
    int depth = 0;
    for (size_t i = 0; i < inner.size(); ++i) {
        if (inner[i] == '[') ++depth;
        else if (inner[i] == ']') --depth;
        else if (inner[i] == ',' && depth == 0) {
            size_t start = i + 1;
            while (start < inner.size() && inner[start] == ' ') ++start;
            return inner.substr(start);
        }
    }
    return "";
}

int64_t Type::array_size() const {
    // Fast path: use proto-supplied field when available.
    if (stored_array_size_ >= 0) return stored_array_size_;
    for (const Type* t = this; t; t = t->parent()) {
        if (t->kind_ != Kind::Array) continue;  // only Array names can parse
        int64_t s = parse_array_size(t->name_);
        if (s >= 0) return s;
    }
    return -1;
}

std::string Type::array_element_type_name() const {
    // Fast path: use proto-supplied field when available.
    if (!element_type_name_.empty()) return element_type_name_;
    for (const Type* t = this; t; t = t->parent()) {
        if (t->kind_ != Kind::Array) continue;  // only Array names can parse
        std::string elem = parse_array_elem_type(t->name_);
        if (!elem.empty()) return elem;
    }
    return "";
}

const Type* Type::bounded_int_ancestor() const { // [XTS]
    const Type* current = this;
    while (current) {
        if (current->is_bounded_int()) return current;
        current = current->parent();
    }
    return nullptr;
}

} // namespace rantanplan
