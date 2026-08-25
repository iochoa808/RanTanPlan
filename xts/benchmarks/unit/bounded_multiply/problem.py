"""
Area calculator — valid multiplication: compute area = width * height.
set_height raises the height parameter; compute multiplies two bounded-int fluents.
Initial: width=2, height=1, area=0.  Goal: area=6.
Plan: set_height(3), compute  →  area = 2*3 = 6.
PDDL-XTS: bounded_multiply
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('area_calculator')

    width_t  = IntType(1, 3)
    height_t = IntType(1, 3)
    area_t   = IntType(0, 9)   # max = 3*3 = 9, stays in range

    width  = Fluent('width',  width_t)
    height = Fluent('height', height_t)
    area   = Fluent('area',   area_t)
    p.add_fluent(width,  default_initial_value=1)
    p.add_fluent(height, default_initial_value=1)
    p.add_fluent(area,   default_initial_value=0)

    p.set_initial_value(width,  2)
    p.set_initial_value(height, 1)
    p.set_initial_value(area,   0)

    # set_height(h): raise height to h (param-assignment, same as bounded_params)
    set_height = InstantaneousAction('set_height', h=height_t)
    h = set_height.parameter('h')
    set_height.add_precondition(LT(height, h))
    set_height.add_effect(height, h)
    p.add_action(set_height)

    # compute: assign area = width * height  (multiplication of two bounded-int fluents)
    compute = InstantaneousAction('compute')
    compute.add_precondition(Equals(area, 0))
    compute.add_effect(area, Times(width, height))
    p.add_action(compute)

    p.add_goal(Equals(area, 6))
    return p