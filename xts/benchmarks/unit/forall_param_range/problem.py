"""
Forall param range — forall with param upper bound (mark_prefix) AND param lower+upper
bounds (check_window). Both forms needed to reach the goal.
Initial cells = [0, 5, 7, 0, 0]; goal: prefix_done AND window_clear.
Plan: zero(1), zero(2), mark_prefix(4), check_window(1, 2).
PDDL-XTS: forall_param_range
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('forall_param_array')

    idx_t = IntType(0, 4)
    val_t = IntType(0, 9)

    cells        = Fluent('cells',        ArrayType(5, val_t))
    prefix_done  = Fluent('prefix_done')
    window_clear = Fluent('window_clear')
    p.add_fluent(cells)
    p.add_fluent(prefix_done,  default_initial_value=False)
    p.add_fluent(window_clear, default_initial_value=False)

    p.set_initial_value(cells, [0, 5, 7, 0, 0])

    zero = InstantaneousAction('zero', i=idx_t)
    i = zero.parameter('i')
    zero.add_precondition(GT(cells[i], 0))
    zero.add_effect(cells[i], 0)
    p.add_action(zero)

    em = get_environment().expression_manager

    # mark_prefix(n): forall i in (0..n) — param upper bound only
    mark_prefix = InstantaneousAction('mark_prefix', n=idx_t)
    n = mark_prefix.parameter('n')
    rv = RangeVariable('i', 0, n)
    mark_prefix.add_precondition(
        Forall(Equals(em.ArrayRead(em.FluentExp(cells), em.RangeVariableExp(rv)), 0), rv))
    mark_prefix.add_effect(prefix_done, True)
    p.add_action(mark_prefix)

    # check_window(lo, hi): forall i in (lo..hi) — param LOWER AND upper bound
    check_window = InstantaneousAction('check_window', lo=idx_t, hi=idx_t)
    lo, hi = check_window.parameter('lo'), check_window.parameter('hi')
    rv2 = RangeVariable('i', lo, hi)
    check_window.add_precondition(LE(lo, hi))
    check_window.add_precondition(
        Forall(Equals(em.ArrayRead(em.FluentExp(cells), em.RangeVariableExp(rv2)), 0), rv2))
    check_window.add_effect(window_clear, True)
    p.add_action(check_window)

    p.add_goal(prefix_done)
    p.add_goal(window_clear)
    return p
