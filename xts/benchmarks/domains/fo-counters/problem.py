"""
FO-Counters — 4-counter chain (tractable instance; original i18 has 18 counters).

Each counter has value in [0,36] and rate in [0,10].
Goal: c0+1 ≤ c1, c1+1 ≤ c2, c2+1 ≤ c3  (minimum satisfying: values [0,1,2,3]).
PDDL-XTS domain: PDDL-XTS/fo-counters/domain.pddl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    n = 4
    p = Problem('fo_counters_4')

    Counter = UserType('Counter')
    counters = [Object(f'c{i}', Counter) for i in range(n)]
    p.add_objects(counters)

    val_t  = IntType(0, 36)
    rate_t = IntType(0, 10)
    value      = Fluent('value',      val_t,  c=Counter)
    rate_value = Fluent('rate_value', rate_t, c=Counter)
    p.add_fluent(value,      default_initial_value=0)
    p.add_fluent(rate_value, default_initial_value=0)

    for c in counters:
        p.set_initial_value(value(c),      0)
        p.set_initial_value(rate_value(c), 0)

    increment = InstantaneousAction('increment', c=Counter)
    c = increment.parameter('c')
    increment.add_precondition(LE(Plus(value(c), rate_value(c)), 36))
    increment.add_effect(value(c), Plus(value(c), rate_value(c)))
    p.add_action(increment)

    decrement = InstantaneousAction('decrement', c=Counter)
    c = decrement.parameter('c')
    decrement.add_precondition(GE(Minus(value(c), rate_value(c)), 0))
    decrement.add_effect(value(c), Minus(value(c), rate_value(c)))
    p.add_action(decrement)

    increase_rate = InstantaneousAction('increase_rate', c=Counter)
    c = increase_rate.parameter('c')
    increase_rate.add_precondition(LT(rate_value(c), 10))
    increase_rate.add_effect(rate_value(c), Plus(rate_value(c), 1))
    p.add_action(increase_rate)

    decrement_rate = InstantaneousAction('decrement_rate', c=Counter)
    c = decrement_rate.parameter('c')
    decrement_rate.add_precondition(GT(rate_value(c), 0))
    decrement_rate.add_effect(rate_value(c), Minus(rate_value(c), 1))
    p.add_action(decrement_rate)

    # Chain goal: c0+1 ≤ c1+1 ≤ … ≤ c(n-1)
    for i in range(n - 1):
        p.add_goal(LE(Plus(value(counters[i]), 1), value(counters[i + 1])))

    return p
