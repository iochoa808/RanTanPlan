"""
incremental-cascade: x=0, y=0; goal y>=50. Use direct-set -> 1 step.
PDDL-XTS: incremental-cascade
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem("incremental-cascade-1-xts")

    val_t = IntType(0, 100)

    g = Fluent("g")
    x = Fluent("x", val_t)
    y = Fluent("y", val_t)

    p.add_fluent(g, default_initial_value=False)
    p.add_fluent(x, default_initial_value=0)
    p.add_fluent(y, default_initial_value=0)

    p.set_initial_value(x, Int(0))
    p.set_initial_value(y, Int(0))

    waste = InstantaneousAction("waste")
    waste.add_effect(x, Int(100))
    p.add_action(waste)

    bridge = InstantaneousAction("bridge")
    bridge.add_precondition(g)
    bridge.add_effect(y, x)
    p.add_action(bridge)

    trigger = InstantaneousAction("trigger")
    trigger.add_effect(g, True)
    p.add_action(trigger)

    direct_set = InstantaneousAction("direct-set")
    direct_set.add_effect(y, Int(50))
    p.add_action(direct_set)

    p.add_goal(GE(y, Int(50)))
    return p
