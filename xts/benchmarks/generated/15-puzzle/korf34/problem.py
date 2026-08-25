"""15-Puzzle Korf instance korf34 -- see PDDL-XTS/generators/15-puzzle/korf_15puzzle.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from korf_15puzzle import build_15puzzle, BOARDS


def get_problem():
    return build_15puzzle('korf34', BOARDS['korf34'])
