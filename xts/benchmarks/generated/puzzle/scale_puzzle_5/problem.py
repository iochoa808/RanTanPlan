"""Puzzle 5x5 — size-sweep instance, see PDDL-XTS/generators/puzzle/puzzle_scaling.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from puzzle_scaling import build_puzzle


def get_problem():
    return build_puzzle(5)
