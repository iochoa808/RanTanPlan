"""
Tag row — 1D array of obj-sets; union tags between slots.
PDDL-XTS: 1d_array_objsets
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('tag_row')

    Tag = UserType('Tag')
    red   = Object('red',   Tag)
    blue  = Object('blue',  Tag)
    green = Object('green', Tag)
    p.add_objects([red, blue, green])

    slot_t = IntType(0, 2)

    rooms = Fluent('rooms', ArrayType(3, SetType(Tag)))
    p.add_fluent(rooms)
    p.set_initial_value(rooms, [{red, blue}, {green}, {red}])

    gather = InstantaneousAction('gather', src=slot_t, dst=slot_t)
    src, dst = gather.parameter('src'), gather.parameter('dst')
    gather.add_precondition(Not(Equals(src, dst)))
    gather.add_effect(rooms[dst], SetUnion(rooms[dst], rooms[src]))
    p.add_action(gather)

    p.add_goal(SetMember(red,   rooms[2]))
    p.add_goal(SetMember(green, rooms[2]))
    return p
