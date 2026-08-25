"""
sdac-zero-bound: identical to sdac-simple but counter starts at 0.
Plan: increment(c0) x3, finish(c0) = 4 steps.
PDDL-XTS: sdac-zero-bound
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem("sdac-zero-bound-1-xts")

    Counter = UserType("counter")
    cval_t  = IntType(0, 11)

    done      = Fluent("done")
    is_target = Fluent("is-target", c=Counter)
    value     = Fluent("value", cval_t, c=Counter)

    p.add_fluent(done,      default_initial_value=False)
    p.add_fluent(is_target, default_initial_value=False)
    p.add_fluent(value,     default_initial_value=0)

    c0 = Object("c0", Counter)
    p.add_object(c0)

    p.set_initial_value(value(c0),     Int(0))
    p.set_initial_value(is_target(c0), True)

    increment = InstantaneousAction("increment", c=Counter)
    c = increment.parameter("c")
    increment.add_precondition(LE(value(c), Int(10)))
    increment.add_effect(value(c), Plus(value(c), Int(1)))
    p.add_action(increment)

    finish = InstantaneousAction("finish", c=Counter)
    c2 = finish.parameter("c")
    finish.add_precondition(is_target(c2))
    finish.add_precondition(GE(value(c2), Int(3)))
    finish.add_effect(done, True)
    p.add_action(finish)

    p.add_goal(done)
    return p
