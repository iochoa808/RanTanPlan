"""
Forall when-guard — (when forall_body effect): check marks done only if all cells=0.
reset(i): cells[i]=0.  check: if forall i in [0..2]: cells[i]=0 → done.
Initial: arr=[0,5,0]; goal: done.
Plan: reset(1), check()  →  2 steps.
PDDL-XTS: when_forall_guard
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('forall_when_guard')

    idx_t = IntType(0, 2)
    val_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(3, val_t))
    done  = Fluent('done')
    p.add_fluent(cells)
    p.add_fluent(done, default_initial_value=False)

    p.set_initial_value(cells, [0, 5, 0])
    p.set_initial_value(done,  False)

    reset = InstantaneousAction('reset', i=idx_t)
    i = reset.parameter('i')
    reset.add_precondition(GT(cells[i], 0))
    reset.add_effect(cells[i], Int(0))
    p.add_action(reset)

    rv = RangeVariable('i', 0, 2)
    forall_guard = Forall(Equals(cells[rv], 0), rv)
    check = InstantaneousAction('check')
    check.add_precondition(Not(done))
    check.add_effect(done, True, condition=forall_guard)
    p.add_action(check)

    p.add_goal(done)
    return p
