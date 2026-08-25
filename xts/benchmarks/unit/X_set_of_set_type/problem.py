"""Declaring a set-of-sets type (SetType of SetType) which is not supported. PDDL-XTS: X_set_of_set_type"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_set_of_set_type')

    Tag     = UserType('Tag')
    # Error: SetType(SetType(...)) — set of sets is not a valid type in UP
    tagset_t   = SetType(Tag)
    powerset_t = SetType(tagset_t)

    red   = Object('red',   Tag)
    blue  = Object('blue',  Tag)
    green = Object('green', Tag)
    p.add_objects([red, blue, green])

    library = Fluent('library', powerset_t)
    p.add_fluent(library, default_initial_value=set())

    p.add_goal(BoolConstant(True))
    return p