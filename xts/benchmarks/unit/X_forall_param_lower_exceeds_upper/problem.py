"""Forall with param lower bound always > upper — always-empty range → goal unreachable. PDDL-XTS: X_forall_param_lower_exceeds_upper"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_forall_lb_gt_ub')

    lo_t  = IntType(4, 5)   # lower bound always >= 4
    val_t = IntType(0, 1)

    cells = Fluent('cells', ArrayType(4, val_t))
    p.add_fluent(cells)
    p.set_initial_value(cells, [0, 0, 0, 0])

    # forall range: lo (param, type [4,5]) to 3 (constant) → always inverted → always empty
    zero_cells = InstantaneousAction('zero_cells', lo=lo_t)
    lo = zero_cells.parameter('lo')
    # RangeVariable(lo, 3): lo is always > 3, so the range is empty
    rv = RangeVariable('i', lo, 3)
    zero_cells.add_effect(cells[rv], Int(1), forall=[rv])
    p.add_action(zero_cells)

    # Goal unreachable because cells never gets written to 1
    p.add_goal(Equals(cells[0], 1))
    return p
