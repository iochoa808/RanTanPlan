#pragma once

namespace planmt {

/**
 * @brief Enum for selecting propagator strategy types
 */
enum class PropagatorType {
    NULL_PROPAGATOR,   // No propagation (default)
    FORALL,           // Forall-specific propagation
    EXISTS            // Exists-specific propagation with incremental cycle detection
};

} // namespace planmt