"""
Constant-index writes on a 3D array — three-bracket IPAR cell SVs ("cube[0][1][0]").
PDDL-XTS: write_3d_const_cells
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('write_3d_const_cells')
    val_t = IntType(0, 9)

    cube = Fluent('cube', ArrayType(2, ArrayType(2, ArrayType(2, val_t))))
    p.add_fluent(cube)
    p.set_initial_value(cube, [[[0, 0], [0, 0]], [[0, 0], [0, 0]]])

    poke_a = InstantaneousAction('poke_a')
    poke_a.add_precondition(Equals(cube[0][1][0], 0))
    poke_a.add_effect(cube[0][1][0], 4)
    p.add_action(poke_a)

    poke_b = InstantaneousAction('poke_b')
    poke_b.add_precondition(Equals(cube[0][1][0], 4))
    poke_b.add_precondition(Equals(cube[1][0][1], 0))
    poke_b.add_effect(cube[1][0][1], 9)
    p.add_action(poke_b)

    p.add_goal(Equals(cube[0][1][0], 4))
    p.add_goal(Equals(cube[1][0][1], 9))
    p.add_goal(Equals(cube[1][1][1], 0))
    return p
