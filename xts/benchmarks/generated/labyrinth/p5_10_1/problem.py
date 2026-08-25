"""Labyrinth IPC instance p5_10_1 -- see PDDL-XTS/generators/labyrinth/labyrinth_ipc.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from labyrinth_ipc import build_labyrinth, INSTANCES


def get_problem():
    n, grid, paths = INSTANCES['p5_10_1']
    return build_labyrinth('p5_10_1', n, grid, paths)
