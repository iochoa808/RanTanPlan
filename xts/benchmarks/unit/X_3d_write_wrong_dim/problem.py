"""Only two indices applied to a 3D tensor (missing one dimension). PDDL-XTS: X_3d_write_wrong_dim"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_3d_write_wrong_dim')

    dim_t = IntType(0, 1)
    val_t = IntType(0, 5)

    tensor = Fluent('tensor', ArrayType(2, ArrayType(2, ArrayType(2, val_t))))
    result = Fluent('result', val_t)
    p.add_fluent(tensor)
    p.add_fluent(result, default_initial_value=0)
    p.set_initial_value(tensor, [[[0,0],[0,0]],[[0,0],[0,0]]])

    # Error: two indices applied to 3D array → tensor[d][r] is still ArrayType, not scalar
    bad_access = InstantaneousAction('bad_access', d=dim_t, r=dim_t, v=val_t)
    d, r, v = bad_access.parameter('d'), bad_access.parameter('r'), bad_access.parameter('v')
    bad_access.add_precondition(Equals(tensor[d][r], 0))
    bad_access.add_effect(tensor[d][r], v)
    p.add_action(bad_access)

    p.add_goal(Equals(result, 5))
    return p