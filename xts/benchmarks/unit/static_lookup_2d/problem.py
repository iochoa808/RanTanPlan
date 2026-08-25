"""
Distance lookup — static 2D table used in precondition and effect.
PDDL-XTS: static_lookup_2d
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('static_lookup')

    node_t = IntType(0, 2)
    dist_t = IntType(0, 9)

    dist_table = Fluent('dist_table', ArrayType(3, ArrayType(3, dist_t)))
    cost       = Fluent('cost',       dist_t)
    p.add_fluent(dist_table)
    p.add_fluent(cost, default_initial_value=0)

    p.set_initial_value(dist_table, [[1,3,8],[2,7,4],[3,5,6]])
    p.set_initial_value(cost, 0)

    record = InstantaneousAction('record', i=node_t, j=node_t)
    i, j = record.parameter('i'), record.parameter('j')
    record.add_precondition(Equals(cost, 0))
    record.add_precondition(GT(dist_table[i][j], 5))
    record.add_effect(cost, dist_table[i][j])
    p.add_action(record)

    p.add_goal(Equals(cost, 7))
    return p
