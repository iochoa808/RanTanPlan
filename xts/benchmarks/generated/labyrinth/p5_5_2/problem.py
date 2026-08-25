"""Labyrinth IPC instance p5_5_2 -- see PDDL-XTS/generators/labyrinth/labyrinth_ipc.py."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from labyrinth_ipc import build_labyrinth, INSTANCES


def get_problem():
    n, grid, paths = INSTANCES['p5_5_2']
    return build_labyrinth('p5_5_2', n, grid, paths)
