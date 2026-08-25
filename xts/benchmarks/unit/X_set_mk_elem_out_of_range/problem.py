"""Set initialisation with elements outside the element type bounds. PDDL-XTS: X_set_mk_elem_out_of_range"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_set_mk_elem_out_of_range')

    altitude_t   = IntType(0, 9)
    altitudeset  = SetType(altitude_t)

    active     = Fluent('active',     altitudeset)
    restricted = Fluent('restricted', altitudeset)
    p.add_fluent(active,     default_initial_value=set())
    p.add_fluent(restricted, default_initial_value=set())

    # Error: Int(15) > ub=9 in active; Int(-1) < lb=0 in restricted
    p.set_initial_value(active,     {Int(1), Int(3), Int(15)})
    p.set_initial_value(restricted, {Int(-1), Int(5)})

    p.add_goal(SetMember(Int(15), active))
    return p