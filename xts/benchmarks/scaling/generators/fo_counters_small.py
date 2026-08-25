"""
FO-Counters (small-range) generator.

Same structure as fo_counters.py but with ranges that scale linearly with n:
  val_t  = IntType(0, n+1)   — just large enough that goals [0..n-1] are reachable
  rate_t = IntType(0, 2)     — three rate values: 0, 1, 2

Purpose: show UP compile-time scaling across more data points.
With large fixed ranges (val_t=[0,36]) UP already fails at n=3.
With these ranges the INTEGERS_REMOVING enumeration is (n+2) * 3 per counter
pair, giving a tractable sweep up to n~6 before compile timeout.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    p = Problem(f'fo_counters_small_n{n}')

    Counter = UserType('Counter')
    counters = [Object(f'c{i}', Counter) for i in range(n)]
    p.add_objects(counters)

    max_val  = n + 1
    max_rate = 2
    val_t  = IntType(0, max_val)
    rate_t = IntType(0, max_rate)
    value      = Fluent('value',      val_t,  c=Counter)
    rate_value = Fluent('rate_value', rate_t, c=Counter)
    p.add_fluent(value,      default_initial_value=0)
    p.add_fluent(rate_value, default_initial_value=0)

    for c in counters:
        p.set_initial_value(value(c),      0)
        p.set_initial_value(rate_value(c), 1)

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