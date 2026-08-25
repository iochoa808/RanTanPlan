"""
IPAR boundary pruning — step_right_0 and step_left_3 must be pruned.
PDDL-XTS: ipar_boundary_prune
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    em = get_environment().expression_manager
    p = Problem('ipar_boundary_prune')

    pos_t = IntType(0, 3)
    val_t = IntType(0, 9)

    cursor = Fluent('cursor', pos_t)
    log    = Fluent('log',    ArrayType(4, val_t))
    p.add_fluent(cursor, default_initial_value=0)
    p.add_fluent(log)

    p.set_initial_value(cursor, 0)
    p.set_initial_value(log,    [9, 9, 9, 9])

    # step_right(?i): cursor = i-1 → cursor = i; log[i-1] = i-1
    step_right = InstantaneousAction('step_right', i=pos_t)
    i = step_right.parameter('i')
    i_e = em.ParameterExp(i)
    step_right.add_precondition(Equals(cursor, Minus(i_e, 1)))
    step_right.add_effect(cursor, i_e)
    step_right.add_effect(em.ArrayWrite(em.FluentExp(log), Minus(i_e, 1)), Minus(i_e, 1))
    p.add_action(step_right)

    # step_left(?i): cursor = i+1 → cursor = i; log[i+1] = i+1
    step_left = InstantaneousAction('step_left', i=pos_t)
    i = step_left.parameter('i')
    i_e = em.ParameterExp(i)
    step_left.add_precondition(Equals(cursor, Plus(i_e, 1)))
    step_left.add_effect(cursor, i_e)
    step_left.add_effect(em.ArrayWrite(em.FluentExp(log), Plus(i_e, 1)), Plus(i_e, 1))
    p.add_action(step_left)

    p.add_goal(Equals(cursor, 3))
    p.add_goal(Equals(log[2], 2))
    return p
