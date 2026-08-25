"""
Set merger — write a set expression (union of two reads) into an array cell.
PDDL-XTS: write_setexpr
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('set_merger')

    slot_t = IntType(0, 2)
    val_t  = IntType(0, 4)

    cells = Fluent('cells', ArrayType(3, SetType(val_t)))
    p.add_fluent(cells)
    p.set_initial_value(cells, [{Int(0), Int(1)}, {Int(2), Int(3)}, {Int(4)}])

    merge_into = InstantaneousAction('merge_into', src=slot_t, dst=slot_t)
    src, dst = merge_into.parameter('src'), merge_into.parameter('dst')
    merge_into.add_precondition(Not(Equals(src, dst)))
    merge_into.add_effect(cells[dst], SetUnion(cells[dst], cells[src]))
    p.add_action(merge_into)

    p.add_goal(SetMember(Int(0), cells[2]))
    p.add_goal(SetMember(Int(1), cells[2]))
    return p
