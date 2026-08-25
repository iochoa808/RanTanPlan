"""Rush Hour instance rh3 -- see PDDL-XTS/generators/rush-hour/rush_hour.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from rush_hour import build_rush_hour, INSTANCES


def get_problem():
    return build_rush_hour('rh3', INSTANCES['rh3'])
