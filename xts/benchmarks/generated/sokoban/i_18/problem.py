"""Sokoban instance i_18 -- see PDDL-XTS/generators/sokoban/sokoban.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from sokoban import build_sokoban, LEVELS


def get_problem():
    return build_sokoban('i_18', LEVELS['i_18'])
