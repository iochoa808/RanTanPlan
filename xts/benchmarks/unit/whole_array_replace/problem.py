"""
Whole-array assign — replace the entire tape in one action effect.
PDDL-XTS: whole_array_replace
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    em = get_environment().expression_manager
    p = Problem('whole_replace')
    val_t = IntType(0, 9)

    tape = Fluent('tape', ArrayType(3, val_t))
    p.add_fluent(tape)
    p.set_initial_value(tape, [5, 6, 7])

    zero_tape = InstantaneousAction('zero_tape')
    zero_tape.add_effect(tape, em.Array([Int(0), Int(0), Int(0)]))
    p.add_action(zero_tape)

    load_pattern = InstantaneousAction('load_pattern')
    load_pattern.add_effect(tape, em.Array([Int(1), Int(2), Int(3)]))
    p.add_action(load_pattern)

    p.add_goal(Equals(tape, [1, 2, 3]))
    return p
