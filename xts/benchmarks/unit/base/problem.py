"""
Player board — parameterized array fluent (slots per player), promote best score.
PDDL-XTS: base
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    em = get_environment().expression_manager
    p = Problem('test_features')

    Player = UserType('Player')
    player1 = Object('player1', Player)
    player2 = Object('player2', Player)
    p.add_objects([player1, player2])

    score_t = IntType(0, 4)
    slot_t  = IntType(0, 2)

    slots = Fluent('slots', ArrayType(3, score_t), player=Player)
    best  = Fluent('best',  score_t,               player=Player)
    p.add_fluent(slots)
    p.add_fluent(best, default_initial_value=0)

    p.set_initial_value(slots(player1), [0, 0, 0])
    p.set_initial_value(slots(player2), [0, 0, 0])
    p.set_initial_value(best(player1), 0)
    p.set_initial_value(best(player2), 0)

    fill_slot = InstantaneousAction('fill_slot', player=Player, i=slot_t, v=score_t)
    player, i, v = [fill_slot.parameter(x) for x in ('player', 'i', 'v')]
    fill_slot.add_precondition(GT(v, 0))
    fill_slot.add_precondition(Equals(em.ArrayRead(slots(player), em.ParameterExp(i)), 0))
    fill_slot.add_effect(em.ArrayWrite(slots(player), em.ParameterExp(i)), v)
    p.add_action(fill_slot)

    clear_slot = InstantaneousAction('clear_slot', player=Player, i=slot_t)
    player, i = [clear_slot.parameter(x) for x in ('player', 'i')]
    clear_slot.add_precondition(GT(em.ArrayRead(slots(player), em.ParameterExp(i)), 0))
    clear_slot.add_effect(em.ArrayWrite(slots(player), em.ParameterExp(i)), 0)
    p.add_action(clear_slot)

    promote_best = InstantaneousAction('promote_best', player=Player, i=slot_t)
    player, i = [promote_best.parameter(x) for x in ('player', 'i')]
    promote_best.add_precondition(GT(em.ArrayRead(slots(player), em.ParameterExp(i)), best(player)))
    promote_best.add_effect(best(player), em.ArrayRead(slots(player), em.ParameterExp(i)))
    p.add_action(promote_best)

    p.add_goal(Equals(em.ArrayRead(slots(player1), em.Int(1)), 2))
    p.add_goal(Equals(em.ArrayRead(slots(player1), em.Int(2)), 3))
    p.add_goal(Equals(em.ArrayRead(slots(player2), em.Int(0)), 1))
    p.add_goal(Equals(em.ArrayRead(slots(player2), em.Int(1)), 0))
    p.add_goal(Equals(em.ArrayRead(slots(player2), em.Int(2)), 4))
    p.add_goal(Equals(best(player1), 3))
    p.add_goal(Equals(best(player2), 4))
    return p
