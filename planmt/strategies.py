from dataclasses import dataclass
from typing import Dict, Optional

@dataclass(frozen=True)
class StrategyConfig:
    """Complete strategy configuration"""
    parallelism: str
    propagator: str
    lazy_interference: bool
    description: str

# All valid strategy presets
STRATEGIES: Dict[str, StrategyConfig] = {
    "sequential": StrategyConfig(
        parallelism="sequential",
        propagator="null", 
        lazy_interference=False,
        description="Classic sequential encoding"
    ),
    "forall-basic": StrategyConfig(
        parallelism="forall",
        propagator="null",
        lazy_interference=False, 
        description="Classic Forall-step semantics with pre-computed syntactic interferences"
    ),
    "forall-optimized": StrategyConfig(
        parallelism="forall",
        propagator="forall",
        lazy_interference=False,
        description="Forall-step semantics, with pre-computed interferences and a propagator that propagates neighbours as false" 
    ),
    "forall-lazy": StrategyConfig(
        parallelism="forall",
        propagator="lazy_forall",
        lazy_interference=True,
        description="Forall-step semantics with lazy interference analysis, only propagating neighbours as false"
    ),
    "exists-basic": StrategyConfig(
        parallelism="exists", 
        propagator="null",
        lazy_interference=False,
        description="Classic Exists-step semantics with pre-computed syntactic interferences"
    ),
    "exists-optimized": StrategyConfig(
        parallelism="exists",
        propagator="exists", 
        lazy_interference=True,
        description="Exists-step semantics with lazy interference analysis, throwing a conflict when a cycle is detected"
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