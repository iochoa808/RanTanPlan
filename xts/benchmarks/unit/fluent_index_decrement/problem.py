"""
Score down — read-modify-write with decrement: cells[i] -= 1.
dec(i): cells[i] = cells[i] - 1, guarded by cells[i] > 0.
Initial: cells=[3,0,0]; goal: cells[0]=0.
Plan: dec(0), dec(0), dec(0).
PDDL-XTS: fluent_index_decrement
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('score_down')
    slot_t  = IntType(0, 2)
    score_t = IntType(0, 5)

    cells = Fluent('cells', ArrayType(3, score_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [3, 0, 0])

    dec = InstantaneousAction('dec', i=slot_t)
    i = dec.parameter('i')
    dec.add_precondition(GT(cells[i], 0))
    dec.add_effect(cells[i], Minus(cells[i], 1))
    p.add_action(dec)

    p.add_goal(Equals(cells[Int(0)], 0))
    return p
