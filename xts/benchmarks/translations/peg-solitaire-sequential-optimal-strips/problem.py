"""
Peg Solitaire XTS — minimal 3-in-a-row.
Features: 1D array of 0/1 pegs.
Init: [1,1,0]. Goal: [0,0,1], moving=0.
Plan: jump-new-move(0), end-move  (2 steps).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('minimal-pegsolitaire-xts')

    idx_t   = IntType(0, 2)
    occ_t   = IntType(0, 1)
    flag_t  = IntType(0, 1)

    pegs     = Fluent('pegs',     ArrayType(3, occ_t))
    moving   = Fluent('moving',   flag_t)
    last_pos = Fluent('last_pos', idx_t)

    p.add_fluent(pegs)
    p.add_fluent(moving,   default_initial_value=Int(0))
    p.add_fluent(last_pos, default_initial_value=Int(0))

    # Init: pegs = [1, 1, 0]
    p.set_initial_value(pegs, [1, 1, 0])
    p.set_initial_value(moving,   Int(0))
    p.set_initial_value(last_pos, Int(0))

    # jump-new-move(?i): moving=0, pegs[i]=1, pegs[i+1]=1, pegs[i+2]=0
    jnm = InstantaneousAction('jump-new-move', i=idx_t)
    i_p = jnm.parameter('i')
    jnm.add_precondition(Equals(moving, Int(0)))
    jnm.add_precondition(Equals(pegs[i_p], Int(1)))
    jnm.add_precondition(Equals(pegs[Plus(i_p, Int(1))], Int(1)))
    jnm.add_precondition(Equals(pegs[Plus(i_p, Int(2))], Int(0)))
    jnm.add_effect(pegs[i_p],                  Int(0))
    jnm.add_effect(pegs[Plus(i_p, Int(1))],    Int(0))
    jnm.add_effect(pegs[Plus(i_p, Int(2))],    Int(1))
    jnm.add_effect(moving,                      Int(1))
    jnm.add_effect(last_pos,                    Plus(i_p, Int(2)))
    p.add_action(jnm)

    # jump-continue-move(?i): moving=1, last_pos=i, pegs[i]=1, pegs[i+1]=1, pegs[i+2]=0
    jcm = InstantaneousAction('jump-continue-move', i=idx_t)
    i_p = jcm.parameter('i')
    jcm.add_precondition(Equals(moving, Int(1)))
    jcm.add_precondition(Equals(last_pos, i_p))
    jcm.add_precondition(Equals(pegs[i_p], Int(1)))
    jcm.add_precondition(Equals(pegs[Plus(i_p, Int(1))], Int(1)))
    jcm.add_precondition(Equals(pegs[Plus(i_p, Int(2))], Int(0)))
    jcm.add_effect(pegs[i_p],                  Int(0))
    jcm.add_effect(pegs[Plus(i_p, Int(1))],    Int(0))
    jcm.add_effect(pegs[Plus(i_p, Int(2))],    Int(1))
    jcm.add_effect(last_pos,                    Plus(i_p, Int(2)))
    p.add_action(jcm)

    # end-move: moving=1 -> moving=0
    end_move = InstantaneousAction('end-move')
    end_move.add_precondition(Equals(moving, Int(1)))
    end_move.add_effect(moving, Int(0))
    p.add_action(end_move)

    # Goal: pegs = [0,0,1], moving = 0
    p.add_goal(Equals(pegs[Int(0)], Int(0)))
    p.add_goal(Equals(pegs[Int(1)], Int(0)))
    p.add_goal(Equals(pegs[Int(2)], Int(1)))
    p.add_goal(Equals(moving, Int(0)))

    return p
