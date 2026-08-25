"""Plotting instance plt0_3_3_2_1 -- see PDDL-XTS/generators/plotting/plotting.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from plotting import build_plotting, INSTANCES


def get_problem():
    grid, remaining = INSTANCES['plt0_3_3_2_1']
    return build_plotting('plt0_3_3_2_1', grid, remaining)
