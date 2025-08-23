from dataclasses import dataclass
from typing import Dict, Optional

@dataclass(frozen=True)
class StrategyConfig:
    """Complete strategy configuration"""
    parallelism: str
    propagator: str
    interference_analysis: str  # "eager", "lazy", or "semantic"
    encoder: str
    description: str

# All valid strategy presets
STRATEGIES: Dict[str, StrategyConfig] = {
    "sequential": StrategyConfig(
        parallelism="sequential",
        propagator="null", 
        interference_analysis="eager",
        encoder="grounded",
        description="Classic sequential encoding"
    ),
    "forall-basic": StrategyConfig(
        parallelism="forall",
        propagator="null",
        interference_analysis="eager",
        encoder="grounded", 
        description="Classic Forall-step semantics with pre-computed syntactic interferences"
    ),
    "forall-optimized": StrategyConfig(
        parallelism="forall",
        propagator="forall",
        interference_analysis="eager",
        encoder="grounded",
        description="Forall-step semantics, with pre-computed interferences and a propagator that propagates neighbours as false" 
    ),
    "forall-lazy": StrategyConfig(
        parallelism="forall",
        propagator="lazy_forall",
        interference_analysis="lazy",
        encoder="grounded",
        description="Forall-step semantics with lazy interference analysis, only propagating neighbours as false"
    ),
    "exists-basic": StrategyConfig(
        parallelism="exists", 
        propagator="null",
        interference_analysis="eager",
        encoder="grounded",
        description="Classic Exists-step semantics with pre-computed syntactic interferences"
    ),
    "exists-optimized": StrategyConfig(
        parallelism="exists",
        propagator="exists", 
        interference_analysis="lazy",
        encoder="grounded",
        description="Exists-step semantics with lazy interference analysis, throwing a conflict when a cycle is detected"
    ),
    "forall-lazy-semantic": StrategyConfig(
        parallelism="forall",
        propagator="lazy_forall",
        interference_analysis="semantic",
        encoder="chained",
        description="Forall-step semantics with semantic interference analysis (SMT-based), only propagating neighbours as false"
    ),
    "exists-optimized-semantic": StrategyConfig(
        parallelism="exists",
        propagator="exists", 
        interference_analysis="semantic",
        encoder="chained",
        description="Exists-step semantics with semantic interference analysis (SMT-based), throwing a conflict when a cycle is detected"
    ),
#    "sequential-heuristic": StrategyConfig(
#        parallelism="sequential",
#        propagator="heuristic",
#        lazy_interference=False,
#        encoder="reified",
#        description="Sequential encoding with goal-directed decision heuristic"
#    ),

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