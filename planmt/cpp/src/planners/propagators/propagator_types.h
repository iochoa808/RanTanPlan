#pragma once

namespace planmt {

/**
 * @brief Enum for selecting propagator strategy types
 */
enum class PropagatorType {
    NULL_PROPAGATOR,   // No propagation (default)
    FORALL,           // Forall-specific propagation
    FORALL_ON_DEMAND  // Simplified forall propagation without conflict detection
};

} // namespace planmt