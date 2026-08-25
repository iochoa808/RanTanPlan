"""
Nested forall in precondition (valid) — forall i: forall j: grid[i][j]=1.
Confirms that nested forall in preconditions is accepted (unlike in effects).
set(i,j): grid[i][j]=1.  verify: fires when all 4 cells=1.
Initial: grid=[[1,1],[1,0]]; goal: done.
Plan: set(1,1), verify()  →  2 steps.
PDDL-XTS: nested_forall_precond
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('nested_forall_prec')

    idx_t = IntType(0, 1)
    val_t = IntType(0, 1)

    grid = Fluent('grid', ArrayType(2, ArrayType(2, val_t)))
    done = Fluent('done')
    p.add_fluent(grid)
    p.add_fluent(done, default_initial_value=False)

    p.set_initial_value(grid, [[1, 1], [1, 0]])
    p.set_initial_value(done, False)

    set_cell = InstantaneousAction('set', i=idx_t, j=idx_t)
    i, j = set_cell.parameter('i'), set_cell.parameter('j')
    set_cell.add_precondition(Equals(grid[i][j], 0))
    set_cell.add_effect(grid[i][j], Int(1))
    p.add_action(set_cell)

    ri = RangeVariable('i', 0, 1)
    rj = RangeVariable('j', 0, 1)
    verify = InstantaneousAction('verify')
    verify.add_precondition(Not(done))
    verify.add_precondition(Forall(Forall(Equals(grid[ri][rj], 1), rj), ri))
    verify.add_effect(done, True)
    p.add_action(verify)

    p.add_goal(done)
    return p
