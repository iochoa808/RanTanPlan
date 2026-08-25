"""
Threshold stamp — conditional effect guarded by a bounded-integer comparison.
stamp(i) writes 7 into cells[i] only when counter >= 5.
Initial: counter=3, cells=[0,0,0,0].  Goal: cells[0]=7, cells[1]=7.
Plan: inc, inc  →  counter=5,  stamp(0), stamp(1)  →  cells[0]=cells[1]=7.
PDDL-XTS: when_numeric_guard
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('threshold_stamp')

    idx_t = IntType(0, 3)
    val_t = IntType(0, 9)
    cnt_t = IntType(0, 9)

    cells   = Fluent('cells',   ArrayType(4, val_t))
    counter = Fluent('counter', cnt_t)
    p.add_fluent(cells)
    p.add_fluent(counter, default_initial_value=0)

    p.set_initial_value(cells,   [0, 0, 0, 0])
    p.set_initial_value(counter, 3)

    inc = InstantaneousAction('inc')
    inc.add_precondition(LT(counter, 9))
    inc.add_effect(counter, Plus(counter, 1))
    p.add_action(inc)

    # stamp(i): cells[i] = 7 ONLY when counter >= 5  (numeric when-guard)
    stamp = InstantaneousAction('stamp', i=idx_t)
    i = stamp.parameter('i')
    stamp.add_precondition(Equals(cells[i], 0))
    stamp.add_effect(cells[i], Int(7), condition=GE(counter, Int(5)))
    p.add_action(stamp)

    p.add_goal(Equals(cells[0], 7))
    p.add_goal(Equals(cells[1], 7))
    return p
