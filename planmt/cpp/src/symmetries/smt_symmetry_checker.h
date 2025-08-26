#pragma once

#include "../problem/problem.h"
#include "../encoders/lifted_encoding_visitor.h"
#include "../config/config.h"
#include <z3++.h>
#include <string>
#include <vector>
#include <unordered_map>

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
 * @brief Stores symmetry information for a pair of symmetric objects
 */
struct SymmetryInfo {
    ObjectSwap object_swap;
    std::vector<std::pair<const Expression*, const Expression*>> variable_pairs;
    
    SymmetryInfo(const ObjectSwap& swap, 
                 const std::vector<std::pair<const Expression*, const Expression*>>& pairs)
        : object_swap(swap), variable_pairs(pairs) {}
};

/**
 * @brief SMT-based symmetry checker that uses Z3 to verify logical equivalence
 * 
 * This implementation follows Rintanen's approach from "Symmetry Reduction for 
 * SAT Representations of Transition Systems" (ICAPS 2003).
 * 
 * Core principle: Two objects are symmetric if swapping them everywhere in the 
 * problem leaves both the initial state and goals logically equivalent.
 * 
 * To break the symmetry we then have to state:
 * First we check if  we're in a state where it is equivalent to have both objects swapped.
 * That is, if we swap the objects, in every fluent their value is the same.
 * If that is the case, then we can safely assert the lex order between the actions that involve them.
 */
class SMTSymmetryChecker {
private:
    const Problem* problem_;
    z3::context& context_;
    SymbolTable symbol_table_;
    LiftedEncodingVisitor visitor_;
    
    // Storage for detected symmetries
    std::vector<SymmetryInfo> detected_symmetries_;
    
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
    
    
    // ========================================================================
    // PUBLIC INTERFACE FOR ACCESSING SYMMETRY INFORMATION
    // ========================================================================
    
    /**
     * @brief Get all detected symmetries with their associated variable pairs
     * @return Vector of SymmetryInfo containing object swaps and their variable pairs
     */
    const std::vector<SymmetryInfo>& get_all_symmetries() const { return detected_symmetries_; }
    
    /**
     * @brief Get all detected object swaps
     * @return Vector of ObjectSwap structs
     */
    std::vector<ObjectSwap> get_object_swaps() const;
    
    /**
     * @brief Get variable pairs for a specific object swap
     * @param obj1 Name of first object
     * @param obj2 Name of second object
     * @return Vector of variable pairs, or empty if no symmetry exists
     */
    std::vector<std::pair<const Expression*, const Expression*>> get_variable_pairs_for_swap(const std::string& obj1, const std::string& obj2) const;
    
    /**
     * @brief Check if two objects are known to be symmetric (from cached results)
     * @param obj1 Name of first object
     * @param obj2 Name of second object
     * @return True if the objects are symmetric (cached from previous detection)
     */
    bool are_objects_known_symmetric(const std::string& obj1, const std::string& obj2) const;
    
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
    
    /**
     * @brief Check if an expression involves a specific object
     * @param expr The expression to check
     * @param obj_name Name of the object to look for
     * @return True if the expression involves the object
     */
    bool expression_involves_object(const Expression& expr, const std::string& obj_name) const;
    
    /**
     * @brief Check if two expressions are symmetric with respect to object swap
     * @param expr1 First expression
     * @param expr2 Second expression  
     * @param obj1 First object in the swap
     * @param obj2 Second object in the swap
     * @return True if expr2 is the same as expr1 with obj1 and obj2 swapped
     */
    bool are_expressions_symmetric(const Expression& expr1, const Expression& expr2, const std::string& obj1, const std::string& obj2) const;
    
    /**
     * @brief Get pairs of fluents related by the object swap symmetry (internal use)
     * @param obj1 Name of first object
     * @param obj2 Name of second object
     * @return Vector of pairs of pointers to fluents (v, v') where v' is obtained from v by swapping obj1 and obj2
     */
    std::vector<std::pair<const Expression*, const Expression*>> get_symmetric_variable_pairs(const std::string& obj1, const std::string& obj2) const;
    
    
};

} // namespace planmt