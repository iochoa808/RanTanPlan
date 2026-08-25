"""
Exists pure int — (exists i in [0..3]: threshold > i) in a precondition.
Body has no array reads or set membership — pure arithmetic comparison.
inc(): threshold += 1.  unlock: fires when threshold > some i in [0..3].
Initial: threshold=0; goal: done.
Plan: inc(), unlock()  →  2 steps.
PDDL-XTS: exists_pure_int
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('exists_pure')

    cnt_t = IntType(0, 9)

    threshold = Fluent('threshold', cnt_t)
    done      = Fluent('done')
    p.add_fluent(threshold, default_initial_value=0)
    p.add_fluent(done,      default_initial_value=False)
    p.set_initial_value(threshold, 0)
    p.set_initial_value(done,      False)

    inc = InstantaneousAction('inc')
    inc.add_precondition(LT(threshold, 9))
    inc.add_effect(threshold, Plus(threshold, 1))
    p.add_action(inc)

    rv = RangeVariable('i', 0, 3)
    unlock = InstantaneousAction('unlock')
    unlock.add_precondition(Not(done))
    unlock.add_precondition(Exists(GT(threshold, rv), rv))
    unlock.add_effect(done, True)
    p.add_action(unlock)

    p.add_goal(done)
    return p
