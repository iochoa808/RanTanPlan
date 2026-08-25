"""
Sweep — forall body with `when` condition in effect (Guide §5.5).
active[i]=1 flags cells for zeroing; sweep zeros every flagged cell atomically.
Initial: cells=[5,7,3,9], active=[0,1,0,1].  Goal: cells[0]=0, cells[1]=0, cells[3]=0.
Plan: mark(0)  →  active=[1,1,0,1],  sweep  →  cells=[0,0,3,0].
PDDL-XTS: forall_when_effect
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('sweep')

    idx_t  = IntType(0, 3)
    val_t  = IntType(0, 9)
    flag_t = IntType(0, 1)   # 0 = inactive, 1 = active

    cells  = Fluent('cells',  ArrayType(4, val_t))
    active = Fluent('active', ArrayType(4, flag_t))
    p.add_fluent(cells)
    p.add_fluent(active)

    p.set_initial_value(cells,  [5, 7, 3, 9])
    p.set_initial_value(active, [0, 1, 0, 1])

    # mark(i): flag cell i for zeroing
    mark = InstantaneousAction('mark', i=idx_t)
    i = mark.parameter('i')
    mark.add_precondition(Equals(active[i], 0))
    mark.add_effect(active[i], 1)
    p.add_action(mark)

    # sweep: forall i in (0..3): when active[i]=1 → cells[i]=0
    sweep = InstantaneousAction('sweep')
    rv = RangeVariable('i', 0, 3)
    sweep.add_effect(cells[rv], Int(0), condition=Equals(active[rv], Int(1)), forall=[rv])
    p.add_action(sweep)

    p.add_goal(Equals(cells[0], 0))
    p.add_goal(Equals(cells[1], 0))
    p.add_goal(Equals(cells[3], 0))
    return p