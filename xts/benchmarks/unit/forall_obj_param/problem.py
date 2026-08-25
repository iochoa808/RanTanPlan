"""
Forall with object param — action has object param, forall effect uses const range.
PDDL-XTS: forall_obj_param
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('forall_obj_param')

    Agent = UserType('Agent')
    admin = Object('admin', Agent)
    p.add_objects([admin])

    idx_t = IntType(0, 3)
    val_t = IntType(0, 9)

    authorized = Fluent('authorized', a=Agent)
    board      = Fluent('board',      ArrayType(4, val_t))
    p.add_fluent(authorized, default_initial_value=False)
    p.add_fluent(board)

    p.set_initial_value(board, [5, 3, 7, 2])

    authorize = InstantaneousAction('authorize', a=Agent)
    a = authorize.parameter('a')
    authorize.add_precondition(Not(authorized(a)))
    authorize.add_effect(authorized(a), True)
    p.add_action(authorize)

    wipe = InstantaneousAction('wipe', a=Agent)
    a = wipe.parameter('a')
    wipe.add_precondition(authorized(a))
    rv = RangeVariable('i', 0, 3)
    wipe.add_effect(board[rv], 0, forall=[rv])
    p.add_action(wipe)

    p.add_goal(Equals(board[0], 0))
    p.add_goal(Equals(board[1], 0))
    p.add_goal(Equals(board[2], 0))
    p.add_goal(Equals(board[3], 0))
    return p
