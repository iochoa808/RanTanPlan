from dataclasses import dataclass
from typing import Dict, Optional

@dataclass(frozen=True)
class StrategyConfig:
    """Complete strategy configuration"""
    parallelism: str
    propagator: str
    interference_analysis: str  # "eager", "lazy", or "semantic"
    encoder: str
    detect_symmetries: bool
    no_action_removal: bool
    description: str

# All valid strategy presets
STRATEGIES: Dict[str, StrategyConfig] = {
    "seq": StrategyConfig(
        parallelism="sequential",
        propagator="null",
        interference_analysis="eager",
        encoder="grounded",
        detect_symmetries=False,
        no_action_removal=False,
        description="Classic sequential encoding"
    ),

    # FORALL SEMANTICS

    "forall": StrategyConfig(
        parallelism="forall",
        propagator="null",
        interference_analysis="eager",
        encoder="grounded",
        detect_symmetries=False,
        no_action_removal=False,
        description="Classic Forall-step semantics with pre-computed syntactic interferences"
    ),
    "forall-prop": StrategyConfig(
        parallelism="forall",
        propagator="forall",
        interference_analysis="eager",
        encoder="grounded",
        detect_symmetries=False,
        no_action_removal=False,
        description="Forall-step semantics, with pre-computed interferences and a propagator that propagates neighbours as false"
    ),
    "forall-lazy": StrategyConfig(
        parallelism="forall",
        propagator="lazy_forall",
        interference_analysis="lazy",
        encoder="grounded",
        detect_symmetries=False,
        no_action_removal=False,
        description="Forall-step semantics with lazy interference analysis, only propagating neighbours as false"
    ),
    "forall-lazy-semantic": StrategyConfig(
        parallelism="forall",
        propagator="lazy_forall",
        interference_analysis="semantic",
        encoder="grounded",
        detect_symmetries=False,
        no_action_removal=False,
        description="Forall-step semantics with semantic interference analysis, only propagating neighbours as false"
    ),
    "forall-lazy-semantic-chain": StrategyConfig(
        parallelism="forall",
        propagator="lazy_forall",
        interference_analysis="semantic",
        encoder="chained",
        detect_symmetries=False,
        no_action_removal=False,
        description="Forall-step semantics with semantic interference analysis, only propagating neighbours as false and using a chained encoding"
    ),
    "forall-lazy-semantic-chain-symm": StrategyConfig(
        parallelism="forall",
        propagator="lazy_forall",
        interference_analysis="semantic",
        encoder="chained",
        detect_symmetries=True,
        no_action_removal=False,
        description="Forall-step semantics with semantic interference analysis, only propagating neighbours as false and using a chained encoding"
    ),

    # EXISTS SEMANTICS

    "exists": StrategyConfig(
        parallelism="exists",
        propagator="null",
        interference_analysis="eager",
        encoder="grounded",
        detect_symmetries=False,
        no_action_removal=False,
        description="Classic Exists-step semantics with pre-computed syntactic interferences"
    ),
    "exists-lazy": StrategyConfig(
        parallelism="exists",
        propagator="exists",
        interference_analysis="lazy",
        encoder="grounded",
        detect_symmetries=False,
        no_action_removal=False,
        description="Exists-step semantics with lazy interference analysis, throwing a conflict when a cycle is detected"
    ),
    "exists-lazy-semantic": StrategyConfig(
        parallelism="exists",
        propagator="exists",
        interference_analysis="semantic",
        encoder="grounded",
        detect_symmetries=False,
        no_action_removal=False,
        description="Exists-step semantics with semantic interference analysis, throwing a conflict when a cycle is detected"
    ),
    "exists-lazy-semantic-chain": StrategyConfig(
        parallelism="exists",
        propagator="exists",
        interference_analysis="semantic",
        encoder="chained",
        detect_symmetries=False,
        no_action_removal=False,
        description="Exists-step semantics with semantic interference analysis, throwing a conflict when a cycle is detected and using a chained encoding"
    ),
    "exists-lazy-semantic-chain-symm": StrategyConfig(
        parallelism="exists",
        propagator="exists",
        interference_analysis="semantic",
        encoder="chained",
        detect_symmetries=True,
        no_action_removal=False,
        description="Exists-step semantics with semantic interference analysis, throwing a conflict when a cycle is detected and using a chained encoding"
    ),

    "r2e": StrategyConfig(
        parallelism="sequential",
        propagator="null",
        interference_analysis="eager",
        encoder="r2e",
        detect_symmetries=False,
        no_action_removal=False,
        description="R2∃-step semantics with built-in parallelism using declaration order"
    ),

    "dec": StrategyConfig(
        parallelism="exists",
        propagator="heuristic",
        interference_analysis="semantic",
        encoder="chained",
        detect_symmetries=False,
        no_action_removal=False,
        description="Exists-step semantics with semantic interference analysis, throwing a conflict when a cycle is detected and using a chained encoding with decision heuristics"
    ),

    # Variants with action removal disabled

    "exists-lazy-semantic-chain-no-removal": StrategyConfig(
        parallelism="exists",
        propagator="exists",
        interference_analysis="semantic",
        encoder="chained",
        detect_symmetries=False,
        no_action_removal=True,
        description="Exists-step semantics with semantic interference analysis, throwing a conflict when a cycle is detected and using a chained encoding (no action removal)"
    ),

    "r2e-no-removal": StrategyConfig(
        parallelism="sequential",
        propagator="null",
        interference_analysis="eager",
        encoder="r2e",
        detect_symmetries=False,
        no_action_removal=True,
        description="R2∃-step semantics with built-in parallelism using declaration order (no action removal)"
    ),

    "dec-no-removal": StrategyConfig(
        parallelism="exists",
        propagator="heuristic",
        interference_analysis="semantic",
        encoder="chained",
        detect_symmetries=False,
        no_action_removal=True,
        description="Exists-step semantics with semantic interference analysis, throwing a conflict when a cycle is detected and using a chained encoding with decision heuristics (no action removal)"
    ),
}

def get_strategy_config(strategy_name: str) -> StrategyConfig:
    """Get configuration for strategy name"""
    if strategy_name not in STRATEGIES:
        available = list(STRATEGIES.keys())
        raise ValueError(f"Unknown strategy '{strategy_name}'. Available: {available}")
    return STRATEGIES[strategy_name]

def list_strategies() -> Dict[str, str]:
    """Get strategy names and descriptions"""
    return {name: config.description for name, config in STRATEGIES.items()}