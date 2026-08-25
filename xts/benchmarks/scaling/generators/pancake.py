"""
Pancake-sorting generator — parameterized by n (number of pancakes).

Encoding:
  stack = ArrayType(n, IntType(0, n-1))

Action:
  flip(f): for i in [0..f], stack[i] = stack[f-i]   (reverse prefix of length f+1)

Initial state: the reversed permutation [n-1, n-2, …, 1, 0].
Goal:          [0, 1, 2, …, n-1]
Optimal plan:  n-1 flips (flip(n-1), flip(n-2), …, flip(1)).

Why reversed? It is a canonical hard instance (requires maximum flips to sort)
and the plan structure is predictable, making timing comparisons clean.
"""
import sys, os
sys.path.insert(0, os.path.expanduser('~/unified-planning'))
from unified_planning.shortcuts import *


def generate(n: int):
    """Return a pancake-sorting UP Problem with n pancakes."""
    p = Problem(f'pancake_n{n}')

    idx_t = IntType(0, n - 1)
    stack = Fluent('pancake_stack', ArrayType(n, idx_t))
    p.add_fluent(stack)

    # Reversed permutation: hardest canonical instance
    p.set_initial_value(stack, list(range(n - 1, -1, -1)))

    flip = InstantaneousAction('flip', f=idx_t)
    f = flip.parameter('f')
    i = RangeVariable('i', 0, f)
    flip.add_effect(stack[i], stack[f - i], forall=[i])
    p.add_action(flip)

    p.add_goal(Equals(stack, list(range(n))))
    return p