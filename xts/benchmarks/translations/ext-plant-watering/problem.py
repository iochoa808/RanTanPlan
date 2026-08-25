"""
Extended Plant-Watering XTS — water plant1 to level 4, balance loaded/poured.
XTS features: bounded integers (coord, carry, water).
Plan: 11 steps (move_up x2, load x4, move_down_left, pour x4).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('ext_plant_watering_4_1_xts')

    Thing    = UserType('thing')
    Location = UserType('location')
    Agent    = UserType('agent', father=Thing)
    Plant    = UserType('plant', father=Thing)
    Tap      = UserType('tap',   father=Thing)

    coord_t = IntType(1, 4)
    carry_t = IntType(0, 5)
    water_t = IntType(0, 25)

    # Functions
    max_carry    = Fluent('max_carry', carry_t, a=Agent)
    water_reserve = Fluent('water_reserve', water_t)
    x_f          = Fluent('x', coord_t, t=Thing)
    y_f          = Fluent('y', coord_t, t=Thing)
    carrying     = Fluent('carrying', carry_t, a=Agent)
    poured       = Fluent('poured', water_t, p_=Plant)
    total_poured = Fluent('total_poured', water_t)
    total_loaded = Fluent('total_loaded', water_t)
    p.add_fluent(max_carry,     default_initial_value=0)
    p.add_fluent(water_reserve, default_initial_value=0)
    p.add_fluent(x_f,           default_initial_value=1)
    p.add_fluent(y_f,           default_initial_value=1)
    p.add_fluent(carrying,      default_initial_value=0)
    p.add_fluent(poured,        default_initial_value=0)
    p.add_fluent(total_poured,  default_initial_value=0)
    p.add_fluent(total_loaded,  default_initial_value=0)

    # Objects
    tap1   = Object('tap1',   Tap)
    agent1 = Object('agent1', Agent)
    plant1 = Object('plant1', Plant)
    plant2 = Object('plant2', Plant)
    plant3 = Object('plant3', Plant)
    plant4 = Object('plant4', Plant)
    p.add_objects([tap1, agent1, plant1, plant2, plant3, plant4])

    # Initial state
    p.set_initial_value(total_poured, Int(0))
    p.set_initial_value(total_loaded, Int(0))
    p.set_initial_value(water_reserve, Int(25))
    p.set_initial_value(x_f(tap1),   Int(3))
    p.set_initial_value(y_f(tap1),   Int(3))
    p.set_initial_value(x_f(agent1), Int(3))
    p.set_initial_value(y_f(agent1), Int(1))
    p.set_initial_value(carrying(agent1),  Int(0))
    p.set_initial_value(max_carry(agent1), Int(5))
    p.set_initial_value(x_f(plant1), Int(2))
    p.set_initial_value(y_f(plant1), Int(2))
    p.set_initial_value(poured(plant1), Int(0))
    p.set_initial_value(x_f(plant2), Int(1))
    p.set_initial_value(y_f(plant2), Int(1))
    p.set_initial_value(poured(plant2), Int(0))
    p.set_initial_value(x_f(plant3), Int(1))
    p.set_initial_value(y_f(plant3), Int(1))
    p.set_initial_value(poured(plant3), Int(0))
    p.set_initial_value(x_f(plant4), Int(1))
    p.set_initial_value(y_f(plant4), Int(1))
    p.set_initial_value(poured(plant4), Int(0))

    # Move actions
    move_up = InstantaneousAction('move_up', a=Agent)
    a = move_up.parameter('a')
    move_up.add_precondition(LT(y_f(a), Int(4)))
    move_up.add_effect(y_f(a), Plus(y_f(a), Int(1)))
    p.add_action(move_up)

    move_down = InstantaneousAction('move_down', a=Agent)
    a = move_down.parameter('a')
    move_down.add_precondition(GT(y_f(a), Int(1)))
    move_down.add_effect(y_f(a), Minus(y_f(a), Int(1)))
    p.add_action(move_down)

    move_right = InstantaneousAction('move_right', a=Agent)
    a = move_right.parameter('a')
    move_right.add_precondition(LT(x_f(a), Int(4)))
    move_right.add_effect(x_f(a), Plus(x_f(a), Int(1)))
    p.add_action(move_right)

    move_left = InstantaneousAction('move_left', a=Agent)
    a = move_left.parameter('a')
    move_left.add_precondition(GT(x_f(a), Int(1)))
    move_left.add_effect(x_f(a), Minus(x_f(a), Int(1)))
    p.add_action(move_left)

    move_up_left = InstantaneousAction('move_up_left', a=Agent)
    a = move_up_left.parameter('a')
    move_up_left.add_precondition(GT(x_f(a), Int(1)))
    move_up_left.add_precondition(LT(y_f(a), Int(4)))
    move_up_left.add_effect(y_f(a), Plus(y_f(a), Int(1)))
    move_up_left.add_effect(x_f(a), Minus(x_f(a), Int(1)))
    p.add_action(move_up_left)

    move_up_right = InstantaneousAction('move_up_right', a=Agent)
    a = move_up_right.parameter('a')
    move_up_right.add_precondition(LT(x_f(a), Int(4)))
    move_up_right.add_precondition(LT(y_f(a), Int(4)))
    move_up_right.add_effect(y_f(a), Plus(y_f(a), Int(1)))
    move_up_right.add_effect(x_f(a), Plus(x_f(a), Int(1)))
    p.add_action(move_up_right)

    move_down_left = InstantaneousAction('move_down_left', a=Agent)
    a = move_down_left.parameter('a')
    move_down_left.add_precondition(GT(x_f(a), Int(1)))
    move_down_left.add_precondition(GT(y_f(a), Int(1)))
    move_down_left.add_effect(x_f(a), Minus(x_f(a), Int(1)))
    move_down_left.add_effect(y_f(a), Minus(y_f(a), Int(1)))
    p.add_action(move_down_left)

    move_down_right = InstantaneousAction('move_down_right', a=Agent)
    a = move_down_right.parameter('a')
    move_down_right.add_precondition(LT(x_f(a), Int(4)))
    move_down_right.add_precondition(GT(y_f(a), Int(1)))
    move_down_right.add_effect(y_f(a), Minus(y_f(a), Int(1)))
    move_down_right.add_effect(x_f(a), Plus(x_f(a), Int(1)))
    p.add_action(move_down_right)

    # load(?a, ?t)
    load = InstantaneousAction('load', a=Agent, t=Tap)
    a, t = load.parameter('a'), load.parameter('t')
    load.add_precondition(Equals(x_f(a), x_f(t)))
    load.add_precondition(Equals(y_f(a), y_f(t)))
    load.add_precondition(LT(carrying(a), max_carry(a)))
    load.add_precondition(GE(water_reserve, Int(1)))
    load.add_effect(water_reserve, Minus(water_reserve, Int(1)))
    load.add_effect(carrying(a), Plus(carrying(a), Int(1)))
    load.add_effect(total_loaded, Plus(total_loaded, Int(1)))
    p.add_action(load)

    # pour(?a, ?p)
    pour = InstantaneousAction('pour', a=Agent, p_=Plant)
    a, p_ = pour.parameter('a'), pour.parameter('p_')
    pour.add_precondition(Equals(x_f(a), x_f(p_)))
    pour.add_precondition(Equals(y_f(a), y_f(p_)))
    pour.add_precondition(GE(carrying(a), Int(1)))
    pour.add_effect(carrying(a), Minus(carrying(a), Int(1)))
    pour.add_effect(poured(p_), Plus(poured(p_), Int(1)))
    pour.add_effect(total_poured, Plus(total_poured, Int(1)))
    p.add_action(pour)

    # Goals
    p.add_goal(Equals(poured(plant1), Int(4)))
    p.add_goal(Equals(total_poured, total_loaded))

    return p
