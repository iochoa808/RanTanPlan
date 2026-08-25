"""Pancake Sorting instance n25_3 -- see PDDL-XTS/generators/pancake-sorting/pancake_sorting.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from pancake_sorting import build_pancake, INSTANCES


def get_problem():
    return build_pancake('n25_3', INSTANCES['n25_3'])
