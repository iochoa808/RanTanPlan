"""
Exists goal — (exists i in [0..4]: cells[i]=7) as a top-level goal condition.
write(i): set cells[i]=7.
Initial: arr=[0,0,0,0,0]; goal: some cell equals 7.
Plan: write(0)  →  cells[0]=7  →  exists satisfied.
PDDL-XTS: exists_goal
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('exists_goal')

    idx_t = IntType(0, 4)
    val_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(5, val_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [0, 0, 0, 0, 0])

    write = InstantaneousAction('write', i=idx_t)
    i = write.parameter('i')
    write.add_precondition(Equals(cells[i], 0))
    write.add_effect(cells[i], Int(7))
    p.add_action(write)

    em = get_environment().expression_manager
    rv = RangeVariable('i', 0, 4)
    p.add_goal(Exists(Equals(em.ArrayRead(em.FluentExp(cells), em.RangeVariableExp(rv)), 7), rv))
    return p
