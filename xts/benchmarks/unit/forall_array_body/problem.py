"""
Forall array body — forall with array reads in precondition, array writes in effect.
PDDL-XTS: forall_array_body
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('forall_array_body')

    idx_t = IntType(0, 3)
    val_t = IntType(0, 5)

    cells     = Fluent('cells',     ArrayType(4, val_t))
    gate_open = Fluent('gate_open')
    p.add_fluent(cells)
    p.add_fluent(gate_open, default_initial_value=False)

    p.set_initial_value(cells, [2, 1, 3, 1])

    open_gate = InstantaneousAction('open_gate')
    open_gate.add_precondition(Not(gate_open))
    open_gate.add_effect(gate_open, True)
    p.add_action(open_gate)

    reset_all = InstantaneousAction('reset_all')
    rv = RangeVariable('i', 0, 3)
    reset_all.add_precondition(gate_open)
    reset_all.add_precondition(Forall(GT(cells[rv], 0), rv))
    reset_all.add_effect(cells[rv], 0, forall=[rv])
    p.add_action(reset_all)

    p.add_goal(Equals(cells[0], 0))
    p.add_goal(Equals(cells[3], 0))
    return p
