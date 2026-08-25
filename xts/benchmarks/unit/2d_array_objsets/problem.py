"""
Label grid — 2D array of obj-sets; union row-0 tags into row-1 for active columns.
PDDL-XTS: 2d_array_objsets
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('label_grid')

    Tag = UserType('Tag')
    red   = Object('red',   Tag)
    blue  = Object('blue',  Tag)
    green = Object('green', Tag)
    p.add_objects([red, blue, green])

    col_t = IntType(0, 2)

    labels   = Fluent('labels',   ArrayType(2, ArrayType(3, SetType(Tag))))
    active_c = Fluent('active_c', SetType(col_t))
    p.add_fluent(labels)
    p.add_fluent(active_c, default_initial_value=set())

    p.set_initial_value(labels, [
        [{red, blue}, {green},    {blue, green}],
        [set(),       set(),      set()],
    ])
    p.set_initial_value(active_c, {Int(1), Int(2)})

    propagate = InstantaneousAction('propagate', c=col_t)
    c = propagate.parameter('c')
    propagate.add_precondition(SetMember(c, active_c))
    propagate.add_effect(labels[1][c], SetUnion(labels[1][c], labels[0][c]))
    p.add_action(propagate)

    p.add_goal(SetMember(green, labels[1][1]))
    p.add_goal(SetMember(blue,  labels[1][2]))
    return p
