"""Using a ground fluent (not a parameter) as an array index. PDDL-XTS: X_fluent_as_array_index"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_fluent_as_array_index')

    idx_t = IntType(0, 3)
    val_t = IntType(0, 9)

    cells  = Fluent('cells',  ArrayType(4, val_t))
    head   = Fluent('head',   idx_t)
    result = Fluent('result', val_t)
    p.add_fluent(cells)
    p.add_fluent(head,   default_initial_value=2)
    p.add_fluent(result, default_initial_value=0)
    p.set_initial_value(cells, [5, 3, 7, 1])

    # Error: head is a fluent expression, not a parameter — using it as array index
    copy_head = InstantaneousAction('copy_head')
    copy_head.add_effect(result, cells[head])
    p.add_action(copy_head)

    p.add_goal(Equals(result, 7))
    return p