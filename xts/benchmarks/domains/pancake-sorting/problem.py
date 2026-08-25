"""
Pancake Sorting — array encoding, instance i0.

Stack stored as ArrayType(5, IntType(0,4)).
Flip reverses prefix [0..f] simultaneously.
PDDL-XTS counterpart: PDDL-XTS/pancake-sorting/instances/i0.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('pancake_array_i0')

    idx_t = IntType(0, 4)
    stack = Fluent('pancake_stack', ArrayType(5, idx_t))
    p.add_fluent(stack)
    p.set_initial_value(stack, [3, 4, 2, 1, 0])  # i0: sort to [0,1,2,3,4]

    flip = InstantaneousAction('flip', f=idx_t)
    f = flip.parameter('f')
    i = RangeVariable('i', 0, f)
    flip.add_effect(stack[i], stack[f - i], forall=[i])
    p.add_action(flip)

    p.add_goal(Equals(stack, [0, 1, 2, 3, 4]))
    return p
