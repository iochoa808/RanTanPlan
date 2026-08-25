"""Existential quantifier used in a conditional effect condition. PDDL-XTS: X_exists_in_effect"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_exists_in_effect')

    val_t = IntType(0, 9)

    cells     = Fluent('cells',     ArrayType(4, val_t))
    high_found = Fluent('high_found', BoolType())
    p.add_fluent(cells)
    p.add_fluent(high_found, default_initial_value=False)

    # cells = [2,3,1,6]: no element > 7, so existential condition is False
    p.set_initial_value(cells, [2, 3, 1, 6])

    # Attempt: conditional effect whose condition uses Exists over a range variable.
    # If UP rejects Exists in conditional-effect condition → error (REJECTED).
    # If UP accepts it → UNSOLVABLE because no cell > 7 and condition is never met.
    em = get_environment().expression_manager
    pick_high = InstantaneousAction('pick_high')
    rv = RangeVariable('i', 0, 3)
    pick_high.add_conditional_effect(
        high_found,
        True,
        Exists(GT(em.ArrayRead(em.FluentExp(cells), em.RangeVariableExp(rv)), Int(7)), rv)
    )
    p.add_action(pick_high)

    # Goal is unreachable: no cell > 7 so condition never fires
    p.add_goal(high_found)
    return p