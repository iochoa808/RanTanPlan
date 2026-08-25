"""Plotting instance plt0_6_3_4_4 -- see PDDL-XTS/generators/plotting/plotting.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from plotting import build_plotting, INSTANCES


def get_problem():
    grid, remaining = INSTANCES['plt0_6_3_4_4']
    return build_plotting('plt0_6_3_4_4', grid, remaining)
