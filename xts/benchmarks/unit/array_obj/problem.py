"""
Color palette — 1D array of object (color) tokens, swap pairs.
PDDL-XTS: array_obj
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('color_palette')

    Color = UserType('Color')
    red   = Object('red',   Color)
    green = Object('green', Color)
    blue  = Object('blue',  Color)
    p.add_objects([red, green, blue])

    canvas = Fluent('canvas', ArrayType(3, Color))
    p.add_fluent(canvas)
    p.set_initial_value(canvas, [green, blue, red])

    for (i, j) in [(0,1),(1,2),(0,2)]:
        act = InstantaneousAction(f'swap_{i}{j}', ca=Color, cb=Color)
        ca, cb = act.parameter('ca'), act.parameter('cb')
        act.add_precondition(Equals(canvas[i], ca))
        act.add_precondition(Equals(canvas[j], cb))
        act.add_effect(canvas[i], cb)
        act.add_effect(canvas[j], ca)
        p.add_action(act)

    p.add_goal(Equals(canvas, [red, green, blue]))
    return p
