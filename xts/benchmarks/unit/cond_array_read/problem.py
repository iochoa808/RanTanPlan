"""
Scan a 3-cell array; count cells whose value exceeds 5 using a conditional effect.
PDDL-XTS: cond_array_read
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('cond_arrread')

    idx_t = IntType(0, 2)
    val_t = IntType(0, 9)
    cnt_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(3, val_t))
    hits  = Fluent('hits',  cnt_t)
    p.add_fluent(cells)
    p.add_fluent(hits, default_initial_value=Int(0))

    p.set_initial_value(cells, [7, 2, 8])
    p.set_initial_value(hits, 0)

    scan = InstantaneousAction('scan', i=idx_t)
    i = scan.parameter('i')
    scan.add_effect(hits, Plus(hits, Int(1)), GT(cells[i], Int(5)))
    p.add_action(scan)

    p.add_goal(Equals(hits, Int(2)))
    return p