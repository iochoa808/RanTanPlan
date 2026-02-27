#pragma once

#include "../problem/problem.hpp"
#include "../encoders/lifted_encoding_visitor.hpp"
#include "../config/config.hpp"
#include <z3++.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace rantanplan {
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
 * @brief Represents a detected action symmetry (two actions that can be swapped)
 */
struct ActionSwap {
    const Action* action1;
    const Action* action2;
    
    std::string to_string() const;
};


/**
 * @brief Stores symmetry information for a pair of symmetric objects
 */
struct SymmetryInfo {
    ObjectSwap object_swap;
    std::vector<std::pair<ExprID, ExprID>> variable_pairs;
    std::vector<ActionSwap> action_pairs;

    SymmetryInfo(const ObjectSwap& swap,
                 const std::vector<std::pair<ExprID, ExprID>>& pairs,
                 const std::vector<ActionSwap>& actions = {})
        : object_swap(swap), variable_pairs(pairs), action_pairs(actions) {}
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
 * To break the symmetry we then have to state: First we check if  we're in a
 * state where it is equivalent to have both objects swapped.  That is, if we
 * swap the objects, in every fluent their value is the same.  If that is the
 * case, then we can safely assert the lex order between the actions that
 * involve them.
 * 
 * The symmetries are then broken in the encoder by only ordering pairs of
 * operators that differ in one parameter.  Equivalent of Table 1 in the paper.
 * Table 2 (all pairs of actions) could also be done, but in the paper states
 * it's too much for the solver. TODO might want to test that?
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
    SMTSymmetryChecker(const Problem* problem, z3::context& ctx);
    
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
    std::vector<std::pair<ExprID, ExprID>> get_variable_pairs_for_swap(const std::string& obj1, const std::string& obj2) const;
    
    /**
     * @brief Check if two objects are known to be symmetric (from cached results)
     * @param obj1 Name of first object
     * @param obj2 Name of second object
     * @return True if the objects are symmetric (cached from previous detection)
     */
    bool are_objects_known_symmetric(const std::string& obj1, const std::string& obj2) const;
    
    /**
     * @brief Get action pairs for a specific object swap
     * @param obj1 Name of first object
     * @param obj2 Name of second object
     * @return Vector of ActionSwap structs, or empty if no symmetry exists
     */
    std::vector<ActionSwap> get_action_pairs_for_swap(const std::string& obj1, const std::string& obj2) const;
    
private:
    /**
     * @brief Get all objects of the same type (candidates for symmetry)
     */
    std::unordered_map<std::string, std::vector<const Object*>> get_objects_by_type() const;
    
    /**
     * @brief Convert an ExprID to Z3 using the lifted encoding visitor
     * @param eid The ExprID to convert
     * @return Z3 expression, or nullopt if conversion failed
     */
    z3::expr convert_expr_id_to_z3(ExprID eid);

    /**
     * @brief Check if an expression involves a specific object
     * @param eid The ExprID to check
     * @param obj_name Name of the object to look for
     * @return True if the expression involves the object
     */
    bool expression_involves_object(ExprID eid, const std::string& obj_name) const;

    /**
     * @brief Check if two expressions are symmetric with respect to object swap
     * @param eid1 First expression
     * @param eid2 Second expression
     * @param obj1 First object in the swap
     * @param obj2 Second object in the swap
     * @return True if eid2 is the same as eid1 with obj1 and obj2 swapped
     */
    bool are_expressions_symmetric(ExprID eid1, ExprID eid2, const std::string& obj1, const std::string& obj2) const;

    /**
     * @brief Get pairs of fluents related by the object swap symmetry (internal use)
     * @param obj1 Name of first object
     * @param obj2 Name of second object
     * @return Vector of pairs of ExprIDs (v, v') where v' is obtained from v by swapping obj1 and obj2
     */
    std::vector<std::pair<ExprID, ExprID>> get_symmetric_variable_pairs(const std::string& obj1, const std::string& obj2) const;
    
    /**
     * @brief Get pairs of actions related by the object swap symmetry (internal use)
     * @param obj1 Name of first object
     * @param obj2 Name of second object
     * @return Vector of ActionSwap structs representing symmetric action pairs
     */
    std::vector<ActionSwap> get_symmetric_action_pairs(const std::string& obj1, const std::string& obj2) const;
    
    /**
     * @brief Check if two actions are symmetric by signature with respect to object swap
     * @param action1 First action
     * @param action2 Second action
     * @param obj1 First object in the swap
     * @param obj2 Second object in the swap
     * @return True if action2 has the same name and parameters as action1 with obj1 and obj2 swapped
     */
    bool are_actions_symmetric_by_signature(const Action& action1, const Action& action2, const std::string& obj1, const std::string& obj2) const;
    
    
};

} // namespace rantanplan