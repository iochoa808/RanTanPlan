"""
incremental-cascade-3: x=y=z=h=0; goal done via direct-y, direct-z, finish = 3 steps.
PDDL-XTS: incremental-cascade-3
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem("incremental-cascade-3-1-xts")

    val_t = IntType(0, 100)

    g    = Fluent("g")
    g2   = Fluent("g2")
    done = Fluent("done")
    x    = Fluent("x", val_t)
    y    = Fluent("y", val_t)
    z    = Fluent("z", val_t)
    h    = Fluent("h", val_t)

    p.add_fluent(g,    default_initial_value=False)
    p.add_fluent(g2,   default_initial_value=False)
    p.add_fluent(done, default_initial_value=False)
    p.add_fluent(x,    default_initial_value=0)
    p.add_fluent(y,    default_initial_value=0)
    p.add_fluent(z,    default_initial_value=0)
    p.add_fluent(h,    default_initial_value=0)

    p.set_initial_value(x, Int(0))
    p.set_initial_value(y, Int(0))
    p.set_initial_value(z, Int(0))
    p.set_initial_value(h, Int(0))

    waste = InstantaneousAction("waste")
    waste.add_effect(x, Int(100))
    p.add_action(waste)

    bridge = InstantaneousAction("bridge")
    bridge.add_precondition(g)
    bridge.add_effect(y, x)
    p.add_action(bridge)

    trigger = InstantaneousAction("trigger")
    trigger.add_effect(g,    True)
    trigger.add_effect(h, Int(100))
    p.add_action(trigger)

    bridge2 = InstantaneousAction("bridge2")
    bridge2.add_precondition(g2)
    bridge2.add_effect(z, h)
    p.add_action(bridge2)

    trigger2 = InstantaneousAction("trigger2")
    trigger2.add_effect(g2, True)
    p.add_action(trigger2)

    direct_y = InstantaneousAction("direct-y")
    direct_y.add_effect(y, Int(50))
    p.add_action(direct_y)

    direct_z = InstantaneousAction("direct-z")
    direct_z.add_effect(z, Int(50))
    p.add_action(direct_z)

    finish = InstantaneousAction("finish")
    finish.add_precondition(GE(y, Int(50)))
    finish.add_precondition(GE(z, Int(50)))
    finish.add_effect(done, True)
    p.add_action(finish)

    p.add_goal(done)
    return p
