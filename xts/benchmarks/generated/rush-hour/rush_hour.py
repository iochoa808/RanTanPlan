"""
Shared generator for the 10 Rush Hour benchmark instances (hardest instances
from the game -- the actual paper benchmark).

Replicates the UP model in
~/unified-planning/docs/extensions/domains/rush-hour/RushHour.py, generalized
from its hardcoded example to any instance string, with two copy-paste bugs
in the reference script fixed (confirmed by re-reading it closely -- not
guessed):

1. In the reference script's move_truck_left/up/down blocks, the is_truck(v)
   precondition is mistakenly added to move_truck_right every time
   (`move_truck_right.add_precondition(is_truck(v))` instead of
   `move_truck_left`/`up`/`down`). Harmless in practice -- the 3-consecutive-
   same-id occupancy pattern already uniquely identifies trucks -- but
   fixed here to actually target the right action.

2. In the reference script's move_quad_right block, the exclusivity
   precondition is mistakenly added to move_truck_right instead of
   move_quad_right (`move_truck_right.add_precondition(And(Not(is_truck(v)),
   Not(is_car(v))))`). This is NOT harmless: combined with move_truck_right's
   own correct is_truck(v) precondition, it makes move_truck_right's full
   precondition set self-contradictory (is_truck(v) AND NOT is_truck(v)),
   silently disabling rightward truck movement entirely in the reference
   script. Fixed here by correctly targeting move_quad_right.
"""
import math
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))

from unified_planning.shortcuts import *


def build_rush_hour(name: str, instance: str):
    rows = columns = int(math.sqrt(len(instance)))
    undefined = []
    for i, char in enumerate(instance):
        r, c = divmod(i, columns)
        if char == 'x':
            undefined.append((r, c))
    idx = instance.index('X')
    row_goal = idx // rows

    p = Problem(name)

    Vehicle = UserType('Vehicle')
    none = Object('none', Vehicle)
    X = Object('X', Vehicle)
    p.add_objects([none, X])
    occupied = Fluent('occupied', ArrayType(rows, ArrayType(columns, Vehicle)), undefined_positions=undefined)
    is_car = Fluent('is_car', v=Vehicle)
    is_truck = Fluent('is_truck', v=Vehicle)
    p.add_fluent(occupied, default_initial_value=none)
    p.add_fluent(is_car, default_initial_value=False)
    p.add_fluent(is_truck, default_initial_value=False)
    p.set_initial_value(is_car(X), True)

    # Whole-array assignment, not per-cell occupied[r][c] indexing -- the
    # per-cell form (as in the original RushHour.py) hits an
    # "AssertionError: fluent field must be a fluent" in this UP version,
    # same as the Labyrinth card_at bug (see labyrinth_ipc.py). None of the
    # 10 real instances contain a lowercase 'x' (undefined cell), so this
    # assumes a fully-defined rectangular grid; the assert below documents
    # that assumption rather than silently mishandling it.
    assert not undefined, f"{name}: undefined cells present, whole-array path untested for this case"
    grid = [[None] * columns for _ in range(rows)]
    for i, char in enumerate(instance):
        r, c = divmod(i, columns)
        if char == 'o':
            grid[r][c] = none
        else:
            if not p.has_object(char):
                obj = Object(f'{char}', Vehicle)
                p.add_object(obj)
                p.set_initial_value(is_car(obj), instance.count(char) == 2)
                p.set_initial_value(is_truck(obj), instance.count(char) == 3)
            else:
                obj = p.object(char)
            grid[r][c] = obj
    p.set_initial_value(occupied, grid)

    move_car_right = InstantaneousAction('move_car_right', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_car_right.parameter('v'), move_car_right.parameter('r'), move_car_right.parameter('c')
    move_car_right.add_precondition(Not(Equals(v, none)))
    move_car_right.add_precondition(is_car(v))
    move_car_right.add_precondition(And(Equals(occupied[r][c], v), Equals(occupied[r][c + 1], v)))
    move_car_right.add_precondition(Equals(occupied[r][c + 2], none))
    move_car_right.add_effect(occupied[r][c], none)
    move_car_right.add_effect(occupied[r][c + 2], v)

    move_car_left = InstantaneousAction('move_car_left', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_car_left.parameter('v'), move_car_left.parameter('r'), move_car_left.parameter('c')
    move_car_left.add_precondition(Not(Equals(v, none)))
    move_car_left.add_precondition(is_car(v))
    move_car_left.add_precondition(And(Equals(occupied[r][c], v), Equals(occupied[r][c + 1], v)))
    move_car_left.add_precondition(Equals(occupied[r][c - 1], none))
    move_car_left.add_effect(occupied[r][c - 1], v)
    move_car_left.add_effect(occupied[r][c + 1], none)

    move_car_up = InstantaneousAction('move_car_up', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_car_up.parameter('v'), move_car_up.parameter('r'), move_car_up.parameter('c')
    move_car_up.add_precondition(Not(Equals(v, none)))
    move_car_up.add_precondition(is_car(v))
    move_car_up.add_precondition(And(Equals(occupied[r][c], v), Equals(occupied[r + 1][c], v)))
    move_car_up.add_precondition(Equals(occupied[r - 1][c], none))
    move_car_up.add_effect(occupied[r - 1][c], v)
    move_car_up.add_effect(occupied[r + 1][c], none)

    move_car_down = InstantaneousAction('move_car_down', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_car_down.parameter('v'), move_car_down.parameter('r'), move_car_down.parameter('c')
    move_car_down.add_precondition(Not(Equals(v, none)))
    move_car_down.add_precondition(is_car(v))
    move_car_down.add_precondition(And(Equals(occupied[r][c], v), Equals(occupied[r + 1][c], v)))
    move_car_down.add_precondition(Equals(occupied[r + 2][c], none))
    move_car_down.add_effect(occupied[r][c], none)
    move_car_down.add_effect(occupied[r + 2][c], v)

    move_truck_right = InstantaneousAction('move_truck_right', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_truck_right.parameter('v'), move_truck_right.parameter('r'), move_truck_right.parameter('c')
    move_truck_right.add_precondition(Not(Equals(v, none)))
    move_truck_right.add_precondition(is_truck(v))
    move_truck_right.add_precondition(And(Equals(occupied[r][c], v), Equals(occupied[r][c + 1], v), Equals(occupied[r][c + 2], v)))
    move_truck_right.add_precondition(Equals(occupied[r][c + 3], none))
    move_truck_right.add_effect(occupied[r][c], none)
    move_truck_right.add_effect(occupied[r][c + 3], v)

    move_truck_left = InstantaneousAction('move_truck_left', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_truck_left.parameter('v'), move_truck_left.parameter('r'), move_truck_left.parameter('c')
    move_truck_left.add_precondition(Not(Equals(v, none)))
    move_truck_left.add_precondition(is_truck(v))
    move_truck_left.add_precondition(And(Equals(occupied[r][c], v), Equals(occupied[r][c + 1], v), Equals(occupied[r][c + 2], v)))
    move_truck_left.add_precondition(Equals(occupied[r][c - 1], none))
    move_truck_left.add_effect(occupied[r][c + 2], none)
    move_truck_left.add_effect(occupied[r][c - 1], v)

    move_truck_up = InstantaneousAction('move_truck_up', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_truck_up.parameter('v'), move_truck_up.parameter('r'), move_truck_up.parameter('c')
    move_truck_up.add_precondition(Not(Equals(v, none)))
    move_truck_up.add_precondition(is_truck(v))
    move_truck_up.add_precondition(And(Equals(occupied[r][c], v), Equals(occupied[r + 1][c], v), Equals(occupied[r + 2][c], v)))
    move_truck_up.add_precondition(Equals(occupied[r - 1][c], none))
    move_truck_up.add_effect(occupied[r + 2][c], none)
    move_truck_up.add_effect(occupied[r - 1][c], v)

    move_truck_down = InstantaneousAction('move_truck_down', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_truck_down.parameter('v'), move_truck_down.parameter('r'), move_truck_down.parameter('c')
    move_truck_down.add_precondition(Not(Equals(v, none)))
    move_truck_down.add_precondition(is_truck(v))
    move_truck_down.add_precondition(And(Equals(occupied[r][c], v), Equals(occupied[r + 1][c], v), Equals(occupied[r + 2][c], v)))
    move_truck_down.add_precondition(Equals(occupied[r + 3][c], none))
    move_truck_down.add_effect(occupied[r][c], none)
    move_truck_down.add_effect(occupied[r + 3][c], v)

    move_quad_right = InstantaneousAction('move_quad_right', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_quad_right.parameter('v'), move_quad_right.parameter('r'), move_quad_right.parameter('c')
    move_quad_right.add_precondition(Not(Equals(v, none)))
    move_quad_right.add_precondition(And(Not(is_truck(v)), Not(is_car(v))))
    move_quad_right.add_precondition(And(
        Equals(occupied[r][c], v), Equals(occupied[r][c + 1], v),
        Equals(occupied[r + 1][c], v), Equals(occupied[r + 1][c + 1], v)))
    move_quad_right.add_precondition(Equals(occupied[r][c + 2], none))
    move_quad_right.add_precondition(Equals(occupied[r + 1][c + 2], none))
    move_quad_right.add_effect(occupied[r][c], none)
    move_quad_right.add_effect(occupied[r + 1][c], none)
    move_quad_right.add_effect(occupied[r][c + 2], v)
    move_quad_right.add_effect(occupied[r + 1][c + 2], v)

    move_quad_left = InstantaneousAction('move_quad_left', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_quad_left.parameter('v'), move_quad_left.parameter('r'), move_quad_left.parameter('c')
    move_quad_left.add_precondition(Not(Equals(v, none)))
    move_quad_left.add_precondition(And(Not(is_truck(v)), Not(is_car(v))))
    move_quad_left.add_precondition(And(
        Equals(occupied[r][c], v), Equals(occupied[r][c + 1], v),
        Equals(occupied[r + 1][c], v), Equals(occupied[r + 1][c + 1], v)))
    move_quad_left.add_precondition(Equals(occupied[r][c - 1], none))
    move_quad_left.add_precondition(Equals(occupied[r + 1][c - 1], none))
    move_quad_left.add_effect(occupied[r][c + 1], none)
    move_quad_left.add_effect(occupied[r + 1][c + 1], none)
    move_quad_left.add_effect(occupied[r][c - 1], v)
    move_quad_left.add_effect(occupied[r + 1][c - 1], v)

    move_quad_up = InstantaneousAction('move_quad_up', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_quad_up.parameter('v'), move_quad_up.parameter('r'), move_quad_up.parameter('c')
    move_quad_up.add_precondition(Not(Equals(v, none)))
    move_quad_up.add_precondition(And(Not(is_truck(v)), Not(is_car(v))))
    move_quad_up.add_precondition(And(
        Equals(occupied[r][c], v), Equals(occupied[r][c + 1], v),
        Equals(occupied[r + 1][c], v), Equals(occupied[r + 1][c + 1], v)))
    move_quad_up.add_precondition(Equals(occupied[r - 1][c], none))
    move_quad_up.add_precondition(Equals(occupied[r - 1][c + 1], none))
    move_quad_up.add_effect(occupied[r + 1][c], none)
    move_quad_up.add_effect(occupied[r + 1][c + 1], none)
    move_quad_up.add_effect(occupied[r - 1][c], v)
    move_quad_up.add_effect(occupied[r - 1][c + 1], v)

    move_quad_down = InstantaneousAction('move_quad_down', v=Vehicle, r=IntType(0, rows - 1), c=IntType(0, columns - 1))
    v, r, c = move_quad_down.parameter('v'), move_quad_down.parameter('r'), move_quad_down.parameter('c')
    move_quad_down.add_precondition(Not(Equals(v, none)))
    move_quad_down.add_precondition(And(Not(is_truck(v)), Not(is_car(v))))
    move_quad_down.add_precondition(And(
        Equals(occupied[r][c], v), Equals(occupied[r][c + 1], v),
        Equals(occupied[r + 1][c], v), Equals(occupied[r + 1][c + 1], v)))
    move_quad_down.add_precondition(Equals(occupied[r][c + 2], none))
    move_quad_down.add_precondition(Equals(occupied[r + 1][c + 2], none))
    move_quad_down.add_effect(occupied[r][c], none)
    move_quad_down.add_effect(occupied[r + 1][c], none)
    move_quad_down.add_effect(occupied[r][c + 2], v)
    move_quad_down.add_effect(occupied[r + 1][c + 2], v)

    p.add_actions([move_car_right, move_car_left, move_car_down, move_car_up,
                   move_truck_right, move_truck_left, move_truck_down, move_truck_up,
                   move_quad_right, move_quad_left, move_quad_down, move_quad_up])

    p.add_goal(Equals(occupied[row_goal][columns - 1], X))
    p.add_goal(Equals(occupied[row_goal][columns - 2], X))

    costs = {
        move_car_left: Int(1), move_car_right: Int(1), move_car_up: Int(1), move_car_down: Int(1),
        move_truck_right: Int(1), move_truck_left: Int(1), move_truck_up: Int(1), move_truck_down: Int(1),
        move_quad_right: Int(1), move_quad_left: Int(1), move_quad_up: Int(1), move_quad_down: Int(1),
    }
    p.add_quality_metric(MinimizeActionCosts(costs))
    return p


INSTANCES = {
    'rh1': 'AoooBooAoooBCCDoooOOODXXoEFoGGHHEFooIJKKLooIJooLo',
    'rh2': 'ABOOOoCABPPPoCooQDDUUXXQooUUooQooGGooHEFIJooHEFIJ',
    'rh3': 'oABBCCOoADDPoOoXXEPoOQQQEPooooFGGHHooFIIJooKKLLJo',
    'rh4': 'ooooooPoBBoooPooOoXXPooOooUUCCOoQUUADEEQFoADooQFo',
    'rh5': 'ABBCooOAooCooOXXDoooOEFDGGUUEFHHPUUIIJJPKoooooPKo',
    'rh6': 'oooOOOCoooPPPCoQQQUUGXXBoUUGoEBRADDoEFRAooooFRooo',
    'rh7': 'ooAoBCCooAoBooDDEoFGGXXEoFoHUUVVIIHUUVVooJoKKLLoJ',
    'rh8': 'oooooUUAAOBBUUXXOVVoooDOVVCooDEEPCoooooPFFooooPoo',
    'rh9': 'CCFFPoQABooPoQABXXPoQooEDOOOooEDoooGGHHoUUoooooUU',
    'rh10': 'AUUooooAUUoOOOoCXXPoQoCoBPoQoDDBPoQEVVoRRREVVoooo'
}
