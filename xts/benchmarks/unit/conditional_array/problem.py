"""
Conditional array — write to array cell only when chip is live.
PDDL-XTS: conditional_array
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('cond_array')

    Chip = UserType('Chip')
    ok_chip   = Object('ok_chip',   Chip)
    dead_chip = Object('dead_chip', Chip)
    p.add_objects([ok_chip, dead_chip])

    slot_t = IntType(0, 2)
    val_t  = IntType(0, 3)

    live  = Fluent('live',  c=Chip)
    cells = Fluent('cells', ArrayType(3, val_t))
    p.add_fluent(live,  default_initial_value=False)
    p.add_fluent(cells)

    p.set_initial_value(cells,         [0, 0, 0])
    p.set_initial_value(live(ok_chip), True)

    activate = InstantaneousAction('activate', c=Chip, s=slot_t)
    c, s = activate.parameter('c'), activate.parameter('s')
    activate.add_precondition(Equals(cells[s], 0))
    activate.add_effect(cells[s], 3, condition=live(c))
    p.add_action(activate)

    p.add_goal(Equals(cells[1], 3))
    return p
