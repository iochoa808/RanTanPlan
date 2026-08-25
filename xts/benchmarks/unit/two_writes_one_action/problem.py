"""
Two writes in one action — both cells written atomically.
PDDL-XTS: two_writes_one_action
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('two_writes')
    val_t = IntType(0, 9)

    grid = Fluent('grid', ArrayType(4, val_t))
    p.add_fluent(grid)
    p.set_initial_value(grid, [0, 0, 0, 0])

    stamp = InstantaneousAction('stamp')
    stamp.add_precondition(Equals(grid[0], 0))
    stamp.add_precondition(Equals(grid[3], 0))
    stamp.add_effect(grid[0], 1)
    stamp.add_effect(grid[3], 2)
    p.add_action(stamp)

    p.add_goal(Equals(grid[0], 1))
    p.add_goal(Equals(grid[3], 2))
    p.add_goal(Equals(grid[1], 0))
    p.add_goal(Equals(grid[2], 0))
    return p
