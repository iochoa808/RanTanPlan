"""4D array accessed with only 3 or 1 indices — returns sub-arrays, not scalars. PDDL-XTS: X_read_4d_partial_index"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_read_4d_partial')

    d_t   = IntType(0, 1)
    val_t = IntType(0, 9)

    tensor = Fluent('tensor', ArrayType(2, ArrayType(2, ArrayType(2, ArrayType(2, val_t)))))
    result = Fluent('result', val_t)
    p.add_fluent(tensor)
    p.add_fluent(result, default_initial_value=0)

    # Error: tensor[a][b][c] is a 1D-array expression; assigning it to a scalar → type error
    partial3 = InstantaneousAction('partial3', a=d_t, b=d_t, c=d_t)
    a, b, c = partial3.parameter('a'), partial3.parameter('b'), partial3.parameter('c')
    partial3.add_effect(result, tensor[a][b][c])
    p.add_action(partial3)

    # Error: tensor[a] is a 3D-array expression; assigning it to a scalar → type error
    partial1 = InstantaneousAction('partial1', a=d_t)
    a2 = partial1.parameter('a')
    partial1.add_effect(result, tensor[a2])
    p.add_action(partial1)

    p.add_goal(Equals(result, 5))
    return p
