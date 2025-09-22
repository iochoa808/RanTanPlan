#pragma once

#include "grounded_encoder.hpp"

namespace planmt {

/**
 * @brief Chained SMT encoder implementing Section 5.2 from the semantic interference paper
 * 
 * This encoder extends GroundedEncoder to support cumulative effects in parallel plans
 * by implementing the "chained SMT encoding" described in Section 5.2 of the paper
 * "Relaxing non-interference requirements in parallel plans".
 * 
 * For variables modified by multiple actions (|A_n| > 1), it creates intermediate 
 * chain variables ζ^t_{n,i} and applies effects sequentially through the chain.
 * This allows proper handling of cumulative numeric effects from parallel actions.
 */
class ChainedGroundedEncoder : public GroundedEncoder {
public:
    ChainedGroundedEncoder(const Problem& problem, z3::context& ctx);
    
    // Override action encoding to implement chained effects
    std::shared_ptr<z3::expr> encode_actions(int t) override;

private:
    // Map from non-Boolean variable to actions that modify it  
    std::unordered_map<Expression, std::vector<const Action*>> variable_modifiers_;
    
    /**
     * @brief Build index of which actions modify which variables
     */
    void build_variable_modifiers_index();
    
    
    /**
     * @brief Encode chained effects for multi-modified non-Boolean variables
     * Implements equations (7)-(9) from Section 5.2
     */
    std::shared_ptr<z3::expr> encode_chained_effects(int t);
    
    /**
     * @brief Get the effect expression for a specific variable from an action
     */
    std::optional<const EffectExpression*> get_effect_for_variable(
        const Action& action, const Expression& variable) const;
    
    /**
     * @brief Apply an effect to an expression (for chaining)
     * @param effect The effect expression to apply
     * @param base_expr The base expression to apply the effect to  
     * @param timestep The current timestep for variable lookups
     * @return Z3 expression representing effect applied to base_expr
     */
    z3::expr apply_effect_to_expression(const EffectExpression& effect, 
                                      const z3::expr& base_expr, 
                                      int timestep) const;
};

} // namespace planmt