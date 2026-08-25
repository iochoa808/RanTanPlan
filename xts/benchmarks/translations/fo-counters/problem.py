"""
fo-counters XTS (fn-counters-fo, variable-rate).
Features: bounded integers. 2 counters, value [0,4], rate [0,10].
Goal: value(c0)+1 <= value(c1). Expected: 2 steps.
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem("fn_counters_fo_xts_2")

    Counter = UserType("counter")
    c0 = Object("c0", Counter)
    c1 = Object("c1", Counter)
    p.add_objects([c0, c1])

    cval_t  = IntType(0, 4)
    crate_t = IntType(0, 10)
    ccost_t = IntType(0, 50)

    value      = Fluent("value",      cval_t,  c=Counter)
    rate_value = Fluent("rate_value", crate_t, c=Counter)
    total_cost = Fluent("total-cost", ccost_t)

    p.add_fluent(value,      default_initial_value=0)
    p.add_fluent(rate_value, default_initial_value=0)
    p.add_fluent(total_cost, default_initial_value=0)

    p.set_initial_value(value(c0),      Int(0))
    p.set_initial_value(value(c1),      Int(0))
    p.set_initial_value(rate_value(c0), Int(0))
    p.set_initial_value(rate_value(c1), Int(0))
    p.set_initial_value(total_cost,     Int(0))

    increment = InstantaneousAction("increment", c=Counter)
    c_p = increment.parameter("c")
    increment.add_precondition(LE(Plus(value(c_p), rate_value(c_p)), Int(4)))
    increment.add_effect(value(c_p), Plus(value(c_p), rate_value(c_p)))
    increment.add_effect(total_cost, Plus(total_cost, Int(1)))
    p.add_action(increment)

    decrement = InstantaneousAction("decrement", c=Counter)
    c_p = decrement.parameter("c")
    decrement.add_precondition(GE(Minus(value(c_p), rate_value(c_p)), Int(0)))
    decrement.add_effect(value(c_p), Minus(value(c_p), rate_value(c_p)))
    decrement.add_effect(total_cost, Plus(total_cost, Int(1)))
    p.add_action(decrement)

    increase_rate = InstantaneousAction("increase_rate", c=Counter)
    c_p = increase_rate.parameter("c")
    increase_rate.add_precondition(LE(Plus(rate_value(c_p), Int(1)), Int(10)))
    increase_rate.add_effect(rate_value(c_p), Plus(rate_value(c_p), Int(1)))
    increase_rate.add_effect(total_cost, Plus(total_cost, Int(1)))
    p.add_action(increase_rate)

    decrement_rate = InstantaneousAction("decrement_rate", c=Counter)
    c_p = decrement_rate.parameter("c")
    decrement_rate.add_precondition(GE(rate_value(c_p), Int(1)))
    decrement_rate.add_effect(rate_value(c_p), Minus(rate_value(c_p), Int(1)))
    decrement_rate.add_effect(total_cost, Plus(total_cost, Int(1)))
    p.add_action(decrement_rate)

    p.add_goal(LE(Plus(value(c0), Int(1)), value(c1)))

    return p
