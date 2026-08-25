"""
Fn-counters (XTS translation) — 2 bounded-integer counters.

Two counters c0 and c1 start at 0. Value type is (number 0 4).
Goal: c0 + 1 <= c1, so c1 must reach at least 1 while c0 stays at 0.
Minimum plan: increment c1 once.

# Plan: 1 step
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("fn_counters_xts_2")

    Counter = UserType("counter")
    c0 = Object("c0", Counter)
    c1 = Object("c1", Counter)
    p.add_objects([c0, c1])

    cval_t = IntType(0, 4)
    value = Fluent("value", cval_t, c=Counter)
    p.add_fluent(value, default_initial_value=0)
    p.set_initial_value(value(c0), Int(0))
    p.set_initial_value(value(c1), Int(0))

    increment = InstantaneousAction("increment", c=Counter)
    c = increment.parameter("c")
    increment.add_precondition(LT(value(c), Int(4)))
    increment.add_effect(value(c), Plus(value(c), Int(1)))
    p.add_action(increment)

    decrement = InstantaneousAction("decrement", c=Counter)
    c = decrement.parameter("c")
    decrement.add_precondition(GT(value(c), Int(0)))
    decrement.add_effect(value(c), Minus(value(c), Int(1)))
    p.add_action(decrement)

    p.add_goal(LE(Plus(value(c0), Int(1)), value(c1)))

    return p
