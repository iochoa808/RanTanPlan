"""Puzznic instance puzznic44 -- see PDDL-XTS/generators/puzznic/puzznic.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from puzznic import build_puzznic, LEVELS


def get_problem():
    return build_puzznic('puzznic44', LEVELS['puzznic44'])
