"""Nested forall in effect — 2D array zeroed with two range variables. PDDL-XTS: X_nested_forall_effect"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('nested_forall_effect')

    val_t = IntType(0, 5)
    g = Fluent('g', ArrayType(2, ArrayType(2, val_t)))
    p.add_fluent(g)
    p.set_initial_value(g, [[3, 4], [5, 2]])

    zero_all = InstantaneousAction('zero_all')
    ri = RangeVariable('i', 0, 1)
    rj = RangeVariable('j', 0, 1)
    zero_all.add_effect(g[ri][rj], Int(0), forall=[ri, rj])
    p.add_action(zero_all)

    p.add_goal(Equals(g[Int(0)][Int(0)], Int(0)))
    p.add_goal(Equals(g[Int(1)][Int(1)], Int(0)))
    return p