"""
Score counter — read-modify-write: increment array cell in-place.
PDDL-XTS: fluent_index
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('score_counter')
    slot_t  = IntType(0, 2)
    score_t = IntType(0, 5)

    cells = Fluent('cells', ArrayType(3, score_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [1, 3, 0])

    inc = InstantaneousAction('inc', i=slot_t)
    i = inc.parameter('i')
    inc.add_precondition(LT(cells[i], 5))
    inc.add_effect(cells[i], Plus(cells[i], 1))
    p.add_action(inc)

    p.add_goal(Equals(cells[Int(1)], 5))
    return p
