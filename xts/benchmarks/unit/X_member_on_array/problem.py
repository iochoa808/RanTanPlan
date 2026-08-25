"""SetMember applied to an array fluent instead of a set fluent. PDDL-XTS: X_member_on_array"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_member_on_array')

    val_t = IntType(0, 9)

    cells = Fluent('cells', ArrayType(4, val_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [3, 1, 4, 1])

    # Error: cells is an array, not a set — SetMember is invalid here
    find_val = InstantaneousAction('find_val', v=val_t)
    v = find_val.parameter('v')
    find_val.add_precondition(SetMember(v, cells))
    find_val.add_effect(cells[Int(0)], v)
    p.add_action(find_val)

    p.add_goal(Equals(cells[Int(0)], 9))
    return p