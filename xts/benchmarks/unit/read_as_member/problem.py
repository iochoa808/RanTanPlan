"""
Bingo — array cell value used as set-membership element.
PDDL-XTS: read_as_member
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('bingo')

    pos_t   = IntType(0, 3)
    num_t   = IntType(0, 9)
    score_t = IntType(0, 3)

    tiles  = Fluent('tiles',  ArrayType(4, num_t))
    called = Fluent('called', SetType(num_t))
    score  = Fluent('score',  score_t)
    p.add_fluent(tiles)
    p.add_fluent(called, default_initial_value=set())
    p.add_fluent(score, default_initial_value=0)

    p.set_initial_value(tiles,  [7, 3, 9, 2])
    p.set_initial_value(called, {Int(3), Int(9), Int(5)})
    p.set_initial_value(score,  0)

    mark = InstantaneousAction('mark', pos=pos_t, n=num_t)
    pos, n = mark.parameter('pos'), mark.parameter('n')
    mark.add_precondition(Equals(tiles[pos], n))
    mark.add_precondition(SetMember(n, called))
    mark.add_precondition(LT(score, 3))
    mark.add_effect(score, Plus(score, 1))
    p.add_action(mark)

    p.add_goal(Equals(score, 2))
    return p
