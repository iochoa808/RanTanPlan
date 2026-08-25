"""
Cube swap — 3D array of object (Person) elements.
swap_col swaps two persons sharing the same depth+row, in columns 0 and 1.
Initial: cube[d][r][c] = p{4d+2r+c}.  Goal: two pairs swapped.
Plan: swap_col(0,0,p0,p1), swap_col(1,1,p6,p7).
PDDL-XTS: 3d_obj
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('cube_swap')

    Person = UserType('Person')
    persons = [Object(f'p{i}', Person) for i in range(8)]
    p.add_objects(persons)
    p0, p1, p2, p3, p4, p5, p6, p7 = persons

    d_t = IntType(0, 1)
    r_t = IntType(0, 1)

    # 3D array of Person objects: 2×2×2
    cube = Fluent('cube', ArrayType(2, ArrayType(2, ArrayType(2, Person))))
    p.add_fluent(cube)
    p.set_initial_value(cube, [[[p0, p1], [p2, p3]],
                                [[p4, p5], [p6, p7]]])

    # swap_col(d, r, pa, pb): swap cube[d][r][0] and cube[d][r][1]
    swap_col = InstantaneousAction('swap_col', d=d_t, r=r_t, pa=Person, pb=Person)
    d, r, pa, pb = [swap_col.parameter(x) for x in ('d', 'r', 'pa', 'pb')]
    swap_col.add_precondition(Equals(cube[d][r][0], pa))
    swap_col.add_precondition(Equals(cube[d][r][1], pb))
    swap_col.add_effect(cube[d][r][0], pb)
    swap_col.add_effect(cube[d][r][1], pa)
    p.add_action(swap_col)

    p.add_goal(Equals(cube[0][0][0], p1))
    p.add_goal(Equals(cube[0][0][1], p0))
    p.add_goal(Equals(cube[1][1][0], p7))
    p.add_goal(Equals(cube[1][1][1], p6))
    return p
