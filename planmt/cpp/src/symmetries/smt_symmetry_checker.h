#pragma once

#include "../problem/problem.h"
#include "../encoders/lifted_encoding_visitor.h"
#include "../config/config.h"
#include <z3++.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

namespace planmt {

/**
 * @brief Represents a detected object symmetry (two objects that can be swapped)
 */
struct ObjectSwap {
    std::string obj1_name;
    std::string obj2_name; 
    std::string object_type;
    
    std::string to_string() const {
        return obj1_name + " ↔ " + obj2_name + " (type: " + object_type + ")";
    }
};

/**
 * @brief Object name mapping for symmetry checking
 */
using ObjectMapping = std::map<std::string, std::string>;

/**
 * @brief SMT-based symmetry checker that uses Z3 to verify logical equivalence
 * 
 * This implementation follows Rintanen's approach from "Symmetry Reduction for 
 * SAT Representations of Transition Systems" (ICAPS 2003).
 * 
 * Core principle: Two objects are symmetric if swapping them everywhere in the 
 * problem leaves both the initial state and goals logically equivalent.
 */
class SMTSymmetryChecker {
private:
    const Problem* problem_;
    z3::context& context_;
    SymbolTable symbol_table_;
    LiftedEncodingVisitor visitor_;
    
public:
    SMTSymmetryChecker(const Problem* problem, z3::context& ctx)
        : problem_(problem), context_(ctx), symbol_table_(), visitor_(ctx, symbol_table_, problem) {}
    
    /**
     * @brief Detect all symmetric object pairs in the problem
     * @return Vector of detected object swaps
     */
    std::vector<ObjectSwap> detect_all_object_swaps();
    
    /**
     * @brief Check if two specific objects are symmetric using SMT
     * @param obj1 Name of first object
     * @param obj2 Name of second object
     * @return True if objects are symmetric, false otherwise
     */
    bool are_objects_symmetric(const std::string& obj1, const std::string& obj2);
    
private:
    /**
     * @brief Get all objects of the same type (candidates for symmetry)
     */
    std::unordered_map<std::string, std::vector<const Object*>> get_objects_by_type() const;
    
    /**
     * @brief Convert a planMT Expression to Z3 using the lifted encoding visitor
     * @param expr The expression to convert
     * @return Z3 expression, or nullopt if conversion failed
     */
    std::optional<z3::expr> convert_expression_to_z3(const Expression& expr);
    
};

} // namespace planmt