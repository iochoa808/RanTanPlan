"""
Two explicit writes — two separate actions write to different cells.
PDDL-XTS: two_explicit_writes
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('two_explicit_writes')
    val_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(4, val_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [0, 0, 0, 0])

    write_a = InstantaneousAction('write_a')
    write_a.add_precondition(Equals(cells[0], 0))
    write_a.add_effect(cells[0], 5)
    p.add_action(write_a)

    write_b = InstantaneousAction('write_b')
    write_b.add_precondition(Equals(cells[0], 5))
    write_b.add_precondition(Equals(cells[2], 0))
    write_b.add_effect(cells[2], 7)
    p.add_action(write_b)

    p.add_goal(Equals(cells[0], 5))
    p.add_goal(Equals(cells[2], 7))
    p.add_goal(Equals(cells[1], 0))
    p.add_goal(Equals(cells[3], 0))
    return p
