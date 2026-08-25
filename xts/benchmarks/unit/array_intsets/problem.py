"""
Task router — 1D array of int-sets; copy tasks between bays.
PDDL-XTS: array_intsets
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('task_router')

    bay_t  = IntType(0, 2)
    task_t = IntType(0, 4)

    assignments = Fluent('assignments', ArrayType(3, SetType(task_t)))
    p.add_fluent(assignments)
    p.set_initial_value(assignments, [{Int(0), Int(1), Int(2)}, {Int(3), Int(4)}, {Int(0)}])

    copy_tasks = InstantaneousAction('copy_tasks', src=bay_t, dst=bay_t)
    src, dst = copy_tasks.parameter('src'), copy_tasks.parameter('dst')
    copy_tasks.add_precondition(Not(Equals(src, dst)))
    copy_tasks.add_effect(assignments[dst], assignments[src])
    p.add_action(copy_tasks)

    p.add_goal(SetMember(Int(3), assignments[2]))
    p.add_goal(SetMember(Int(4), assignments[2]))
    return p
