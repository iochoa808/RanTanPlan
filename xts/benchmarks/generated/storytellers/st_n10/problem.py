"""Storytellers instance st_n10 -- see PDDL-XTS/generators/storytellers/storytellers.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from storytellers import build_storytellers, SIZES


def get_problem():
    return build_storytellers('st_n10', SIZES['st_n10'])
