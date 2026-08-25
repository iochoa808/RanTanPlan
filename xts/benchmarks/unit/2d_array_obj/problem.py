"""
Seating plan — 2D array of person objects; swap occupants in a row.
PDDL-XTS: 2d_array_obj
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('seating_2d')

    Person = UserType('Person')
    alice = Object('alice', Person)
    bob   = Object('bob',   Person)
    carol = Object('carol', Person)
    dave  = Object('dave',  Person)
    p.add_objects([alice, bob, carol, dave])

    row_t = IntType(0, 1)
    seats = Fluent('seats', ArrayType(2, ArrayType(2, Person)))
    p.add_fluent(seats)
    p.set_initial_value(seats, [[alice, bob], [carol, dave]])

    swap_row = InstantaneousAction('swap_row', r=row_t, ca=Person, cb=Person)
    r, ca, cb = [swap_row.parameter(x) for x in ('r','ca','cb')]
    swap_row.add_precondition(Equals(seats[r][0], ca))
    swap_row.add_precondition(Equals(seats[r][1], cb))
    swap_row.add_effect(seats[r][0], cb)
    swap_row.add_effect(seats[r][1], ca)
    p.add_action(swap_row)

    p.add_goal(Equals(seats[0][0], bob))
    p.add_goal(Equals(seats[0][1], alice))
    return p
