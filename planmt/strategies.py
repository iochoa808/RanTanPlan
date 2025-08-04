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
        description="No parallelism - safest option"
    ),
    "forall-basic": StrategyConfig(
        parallelism="forall",
        propagator="null",
        lazy_interference=False, 
        description="Parallel actions without propagation"
    ),
    "forall-optimized": StrategyConfig(
        parallelism="forall",
        propagator="forall",
        lazy_interference=False,
        description="Parallel actions with forall propagation"  
    ),
    "forall-lazy": StrategyConfig(
        parallelism="forall",
        propagator="lazy_forall",
        lazy_interference=True,
        description="Memory-efficient parallel with lazy propagation"
    ),
    "exists-basic": StrategyConfig(
        parallelism="exists", 
        propagator="null",
        lazy_interference=False,
        description="At-least-one action semantics"
    ),
    "exists-optimized": StrategyConfig(
        parallelism="exists",
        propagator="exists", 
        lazy_interference=True,
        description="Advanced exists with propagation"
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