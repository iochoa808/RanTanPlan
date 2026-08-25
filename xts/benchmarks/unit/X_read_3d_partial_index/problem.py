"""3D array accessed with only 2 indices — returns a 1D sub-array, not a scalar. PDDL-XTS: X_read_3d_partial_index"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_read_3d_partial_index')

    d_t   = IntType(0, 1)
    r_t   = IntType(0, 1)
    val_t = IntType(0, 9)

    tensor = Fluent('tensor', ArrayType(2, ArrayType(2, ArrayType(2, val_t))))
    result = Fluent('result', val_t)
    p.add_fluent(tensor)
    p.add_fluent(result, default_initial_value=0)
    p.set_initial_value(tensor, [[[1, 2], [3, 4]], [[5, 6], [7, 8]]])

    # Error: tensor[d][r] is a 1D-array expression; assigning it to a scalar → type error
    partial_read = InstantaneousAction('partial_read', d=d_t, r=r_t)
    d, r = partial_read.parameter('d'), partial_read.parameter('r')
    partial_read.add_effect(result, tensor[d][r])
    p.add_action(partial_read)

    # Error: assigning scalar 9 to a 1D array row → type error
    partial_write = InstantaneousAction('partial_write', d=d_t, r=r_t)
    d2, r2 = partial_write.parameter('d'), partial_write.parameter('r')
    partial_write.add_effect(tensor[d2][r2], Int(9))
    p.add_action(partial_write)

    p.add_goal(Equals(result, 5))
    return p
