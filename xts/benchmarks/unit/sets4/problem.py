"""
Drone corridor — altitude-set operations for autonomous drone flight.
PDDL-XTS: sets4
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('drone_corridor')

    alt_t      = IntType(0, 9)
    dronecount = IntType(0, 5)

    active_layers     = Fluent('active_layers',     SetType(alt_t))
    restricted_layers = Fluent('restricted_layers', SetType(alt_t))
    charging_layers   = Fluent('charging_layers',   SetType(alt_t))
    ceiling           = Fluent('ceiling',           alt_t)
    traffic_load      = Fluent('traffic_load',      dronecount)

    for f in [active_layers, restricted_layers, charging_layers]:
        p.add_fluent(f, default_initial_value=set())
    p.add_fluent(ceiling,      default_initial_value=0)
    p.add_fluent(traffic_load, default_initial_value=0)

    p.set_initial_value(active_layers,     {Int(1), Int(3)})
    p.set_initial_value(restricted_layers, {Int(5), Int(7)})
    p.set_initial_value(charging_layers,   {Int(2), Int(3), Int(4)})
    p.set_initial_value(ceiling,           4)
    p.set_initial_value(traffic_load,      2)

    enter_layer = InstantaneousAction('enter_layer', a=alt_t)
    a = enter_layer.parameter('a')
    enter_layer.add_precondition(LE(a, ceiling))
    enter_layer.add_precondition(Not(SetMember(a, active_layers)))
    enter_layer.add_precondition(Not(SetMember(a, restricted_layers)))
    enter_layer.add_effect(active_layers,  SetAdd(a, active_layers))
    enter_layer.add_effect(traffic_load,   Plus(traffic_load, 1))
    p.add_action(enter_layer)

    exit_layer = InstantaneousAction('exit_layer', a=alt_t)
    a = exit_layer.parameter('a')
    exit_layer.add_precondition(SetMember(a, active_layers))
    exit_layer.add_effect(active_layers, SetRemove(a, active_layers))
    exit_layer.add_effect(traffic_load,  Minus(traffic_load, 1))
    p.add_action(exit_layer)

    raise_ceiling = InstantaneousAction('raise_ceiling')
    raise_ceiling.add_precondition(LT(ceiling, 9))
    raise_ceiling.add_effect(ceiling, Plus(ceiling, 1))
    p.add_action(raise_ceiling)

    lower_ceiling = InstantaneousAction('lower_ceiling')
    lower_ceiling.add_precondition(GT(ceiling, 0))
    lower_ceiling.add_effect(ceiling, Minus(ceiling, 1))
    p.add_action(lower_ceiling)

    congestion_filter = InstantaneousAction('congestion_filter')
    congestion_filter.add_precondition(GE(SetCardinality(active_layers), 4))
    congestion_filter.add_effect(active_layers, SetIntersection(active_layers, charging_layers))
    p.add_action(congestion_filter)

    activate_charging_grid = InstantaneousAction('activate_charging_grid')
    activate_charging_grid.add_precondition(SetSubseteq(active_layers, charging_layers))
    activate_charging_grid.add_effect(active_layers, SetUnion(active_layers, charging_layers))
    p.add_action(activate_charging_grid)

    coordinated_reroute = InstantaneousAction('coordinated_reroute')
    coordinated_reroute.add_precondition(SetDisjoint(active_layers, restricted_layers))
    coordinated_reroute.add_effect(active_layers, SetUnion(active_layers, restricted_layers))
    p.add_action(coordinated_reroute)

    purge_hazards = InstantaneousAction('purge_hazards')
    purge_hazards.add_effect(active_layers, SetDifference(active_layers, restricted_layers))
    p.add_action(purge_hazards)

    synchronize_grid = InstantaneousAction('synchronize_grid')
    synchronize_grid.add_precondition(Equals(traffic_load, 0))
    synchronize_grid.add_effect(active_layers, charging_layers)
    p.add_action(synchronize_grid)

    p.add_goal(SetSubseteq(active_layers, charging_layers))
    p.add_goal(SetDisjoint(active_layers, restricted_layers))
    p.add_goal(GE(ceiling, 6))
    p.add_goal(LE(traffic_load, 3))
    p.add_goal(SetMember(Int(4), active_layers))
    return p
