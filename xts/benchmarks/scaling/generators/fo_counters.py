"""
FO-Counters generator — parameterized by n (number of counters).

Fluent types:
  val_t  = IntType(0, max_val)   — counter value
  rate_t = IntType(0, max_rate)  — increment rate

Initial state: all values 0, all rates 1.
Goal: c[0]+1 ≤ c[1], c[1]+1 ≤ c[2], …, c[n-2]+1 ≤ c[n-1].
Min satisfying values: [0, 1, 2, …, n-1].
Min plan length: n*(n-1)/2 increment actions.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int, max_val: int = 36, max_rate: int = 10):
    """Return a fo-counters UP Problem with n counters."""
    p = Problem(f'fo_counters_n{n}')

    Counter = UserType('Counter')
    counters = [Object(f'c{i}', Counter) for i in range(n)]
    p.add_objects(counters)

    val_t  = IntType(0, max_val)
    rate_t = IntType(0, max_rate)
    value      = Fluent('value',      val_t,  c=Counter)
    rate_value = Fluent('rate_value', rate_t, c=Counter)
    p.add_fluent(value,      default_initial_value=0)
    p.add_fluent(rate_value, default_initial_value=0)

    for c in counters:
        p.set_initial_value(value(c),      0)
        p.set_initial_value(rate_value(c), 1)   # start with rate=1 to shorten plan

    increment = InstantaneousAction('increment', c=Counter)
    c = increment.parameter('c')
    increment.add_precondition(LE(Plus(value(c), rate_value(c)), max_val))
    increment.add_effect(value(c), Plus(value(c), rate_value(c)))
    p.add_action(increment)

    decrement = InstantaneousAction('decrement', c=Counter)
    c = decrement.parameter('c')
    decrement.add_precondition(GE(Minus(value(c), rate_value(c)), 0))
    decrement.add_effect(value(c), Minus(value(c), rate_value(c)))
    p.add_action(decrement)

    increase_rate = InstantaneousAction('increase_rate', c=Counter)
    c = increase_rate.parameter('c')
    increase_rate.add_precondition(LT(rate_value(c), max_rate))
    increase_rate.add_effect(rate_value(c), Plus(rate_value(c), 1))
    p.add_action(increase_rate)

    decrement_rate = InstantaneousAction('decrement_rate', c=Counter)
    c = decrement_rate.parameter('c')
    decrement_rate.add_precondition(GT(rate_value(c), 0))
    decrement_rate.add_effect(rate_value(c), Minus(rate_value(c), 1))
    p.add_action(decrement_rate)

    for i in range(n - 1):
        p.add_goal(LE(Plus(value(counters[i]), 1), value(counters[i + 1])))

    return p