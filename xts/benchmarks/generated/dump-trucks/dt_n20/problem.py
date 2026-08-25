"""Dump Trucks instance dt_n20 -- see PDDL-XTS/generators/dump-trucks/dump_trucks.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from dump_trucks import build_dump_trucks, SIZES


def get_problem():
    return build_dump_trucks('dt_n20', SIZES['dt_n20'])
