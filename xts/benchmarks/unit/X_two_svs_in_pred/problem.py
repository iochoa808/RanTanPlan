"""Two array read expressions used as arguments to a boolean predicate. PDDL-XTS: X_two_svs_in_pred"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_two_svs_in_pred')

    idx_t = IntType(0, 2)
    Node  = UserType('Node')

    n0 = Object('n0', Node); n1 = Object('n1', Node); n2 = Object('n2', Node)
    n3 = Object('n3', Node); n4 = Object('n4', Node); n5 = Object('n5', Node)
    n6 = Object('n6', Node); n7 = Object('n7', Node); n8 = Object('n8', Node)
    p.add_objects([n0, n1, n2, n3, n4, n5, n6, n7, n8])

    board  = Fluent('board',  ArrayType(3, ArrayType(3, Node)))
    linked = Fluent('linked', BoolType(), a=Node, b=Node)
    p.add_fluent(board)
    p.add_fluent(linked, default_initial_value=False)

    p.set_initial_value(board, [[n0,n1,n2],[n3,n4,n5],[n6,n7,n8]])
    p.set_initial_value(linked(n0, n1), True)
    p.set_initial_value(linked(n4, n5), True)

    # Two array reads as predicate args — precondition uses the two-SVs-in-pred pattern.
    # No effect: mirrors the PDDL domain where (:effect ()) is empty.
    # Goal (linked n3 n7) is never established by any action → UNSOLVABLE.
    check_link = InstantaneousAction('check_link', r=idx_t, c=idx_t, r2=idx_t, c2=idx_t)
    r, c = check_link.parameter('r'), check_link.parameter('c')
    r2, c2 = check_link.parameter('r2'), check_link.parameter('c2')
    check_link.add_precondition(linked(board[r][c], board[r2][c2]))
    p.add_action(check_link)

    p.add_goal(linked(n3, n7))
    return p