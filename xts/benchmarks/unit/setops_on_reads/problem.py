"""
Cell inspector — set ops on 1D array of int-sets (cardinality, subset, intersect).
PDDL-XTS: setops_on_reads
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('cell_inspector')

    slot_t  = IntType(0, 2)
    val_t   = IntType(0, 4)
    count_t = IntType(0, 3)

    cells = Fluent('cells', ArrayType(3, SetType(val_t)))
    big   = Fluent('big',   count_t)
    p.add_fluent(cells)
    p.add_fluent(big, default_initial_value=0)
    p.set_initial_value(cells, [
        {Int(0), Int(1), Int(2), Int(3)},
        {Int(0), Int(2)},
        {Int(1), Int(3), Int(4)},
    ])
    p.set_initial_value(big, 0)

    count_big = InstantaneousAction('count_big', s=slot_t)
    s = count_big.parameter('s')
    count_big.add_precondition(LT(big, 3))
    count_big.add_precondition(GE(SetCardinality(cells[s]), 3))
    count_big.add_effect(big, Plus(big, 1))
    p.add_action(count_big)

    trim_to = InstantaneousAction('trim_to', src=slot_t, dst=slot_t)
    src, dst = trim_to.parameter('src'), trim_to.parameter('dst')
    trim_to.add_precondition(Not(Equals(src, dst)))
    trim_to.add_precondition(Not(SetSubseteq(cells[dst], cells[src])))
    trim_to.add_effect(cells[dst], SetIntersection(cells[dst], cells[src]))
    p.add_action(trim_to)

    p.add_goal(Equals(big, 2))
    return p
