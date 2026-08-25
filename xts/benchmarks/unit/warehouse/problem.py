"""
Warehouse — array of sets (bins) + bounded-int capacities + count-based goal.
PDDL-XTS: arrays, sets, bounded-integers, count

Fixed instance of bench.py's make_warehouse generator (3 bins, 4 items):
bin0={item0,item1} cap2 (already full), bin1={item2} cap2, bin2={item3} cap3.
A bin is "full" once it holds >= 2 items; goal is >= 2 full bins, reached by
moving item3 from bin2 into bin1. Same instance as pddl/test/warehouse and
xts/benchmarks/translations/warehouse, so all three can be diffed side by side.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('warehouse')
    NBINS = 3
    NITEMS = 4
    item_t = IntType(0, NITEMS - 1)
    bin_t = IntType(0, NBINS - 1)
    cap_t = IntType(0, NITEMS)

    bins = Fluent('bins', ArrayType(NBINS, SetType(item_t)))
    capacity = Fluent('capacity', ArrayType(NBINS, cap_t))
    p.add_fluent(bins, default_initial_value=set())
    p.add_fluent(capacity, default_initial_value=0)

    p.set_initial_value(bins, [{0, 1}, {2}, {3}])
    p.set_initial_value(capacity, [2, 2, 3])

    move = InstantaneousAction('move', item=item_t, src=bin_t, dst=bin_t)
    item = move.parameter('item')
    src = move.parameter('src')
    dst = move.parameter('dst')
    move.add_precondition(Not(Equals(src, dst)))
    move.add_precondition(SetMember(item, bins[src]))
    move.add_precondition(LT(SetCardinality(bins[dst]), capacity[dst]))
    move.add_effect(bins[src], SetRemove(item, bins[src]))
    move.add_effect(bins[dst], SetAdd(item, bins[dst]))
    p.add_action(move)

    threshold = 2
    full_bins = 2
    rb = [GE(SetCardinality(bins[i]), threshold) for i in range(NBINS)]
    p.add_goal(GE(Count(rb), full_bins))
    return p