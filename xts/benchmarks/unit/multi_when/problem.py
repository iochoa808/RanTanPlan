"""
Dual gate — two independent conditional effects in one action.
process fires two when-guards independently: flag_a zeros cell_a, flag_b zeros cell_b.
Initial: flag_a=True, flag_b=False, cell_a=5, cell_b=5.
Goal: cell_a=0, cell_b=0.
Plan: activate (sets flag_b=True), process (both guards now true → both cells zeroed).
PDDL-XTS: multi_when
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('dual_gate')

    val_t = IntType(0, 9)

    flag_a = Fluent('flag_a')
    flag_b = Fluent('flag_b')
    cell_a = Fluent('cell_a', val_t)
    cell_b = Fluent('cell_b', val_t)
    p.add_fluent(flag_a, default_initial_value=False)
    p.add_fluent(flag_b, default_initial_value=False)
    p.add_fluent(cell_a, default_initial_value=0)
    p.add_fluent(cell_b, default_initial_value=0)

    p.set_initial_value(flag_a, True)
    p.set_initial_value(flag_b, False)
    p.set_initial_value(cell_a, 5)
    p.set_initial_value(cell_b, 5)

    # activate: enable the second gate
    activate = InstantaneousAction('activate')
    activate.add_precondition(Not(flag_b))
    activate.add_effect(flag_b, True)
    p.add_action(activate)

    # process: two independent when-conditions in one action
    process = InstantaneousAction('process')
    process.add_effect(cell_a, Int(0), condition=flag_a)   # when flag_a: zero cell_a
    process.add_effect(cell_b, Int(0), condition=flag_b)   # when flag_b: zero cell_b
    p.add_action(process)

    p.add_goal(Equals(cell_a, 0))
    p.add_goal(Equals(cell_b, 0))
    return p
