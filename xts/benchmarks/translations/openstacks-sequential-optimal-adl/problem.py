"""
openstacks: o1 waits for p1, o2 waits for p2; start both, make both products, ship both.
Plan: open x2, start x2, make-product x2, ship x2 = 8 steps.
PDDL-XTS: openstacks-sequential-optimal-adl
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *
from unified_planning.model import Variable

def get_problem():
    p = Problem("simple-openstacks-xts")

    Order   = UserType("order")
    Product = UserType("product")
    scount_t = IntType(0, 3)

    includes     = Fluent("includes",  o=Order, pr=Product)
    waiting      = Fluent("waiting",   o=Order)
    started      = Fluent("started",   o=Order)
    shipped      = Fluent("shipped",   o=Order)
    made         = Fluent("made",      pr=Product)
    stacks_avail = Fluent("stacks-avail", scount_t)

    for f in [includes, waiting, started, shipped, made]:
        p.add_fluent(f, default_initial_value=False)
    p.add_fluent(stacks_avail, default_initial_value=0)

    o1 = Object("o1", Order)
    o2 = Object("o2", Order)
    p1 = Object("p1", Product)
    p2 = Object("p2", Product)
    p.add_objects([o1, o2, p1, p2])

    p.set_initial_value(stacks_avail, Int(0))
    p.set_initial_value(waiting(o1),     True)
    p.set_initial_value(includes(o1, p1), True)
    p.set_initial_value(waiting(o2),     True)
    p.set_initial_value(includes(o2, p2), True)

    # make-product(?p): not made, forall ?o: imply(includes(?o,?p), started(?o))
    make_product = InstantaneousAction("make-product", pr=Product)
    pr = make_product.parameter("pr")
    o_var = Variable("o", Order)
    make_product.add_precondition(Not(made(pr)))
    make_product.add_precondition(
        Forall(Implies(includes(o_var, pr), started(o_var)), o_var))
    make_product.add_effect(made(pr), True)
    p.add_action(make_product)

    # start-order(?o): waiting, stacks-avail >= 1
    start_order = InstantaneousAction("start-order", o=Order)
    o = start_order.parameter("o")
    start_order.add_precondition(waiting(o))
    start_order.add_precondition(GE(stacks_avail, Int(1)))
    start_order.add_effect(waiting(o),  False)
    start_order.add_effect(started(o),  True)
    start_order.add_effect(stacks_avail, Minus(stacks_avail, Int(1)))
    p.add_action(start_order)

    # ship-order(?o): started, forall ?p: imply(includes(?o,?p), made(?p)), stacks-avail<3
    ship_order = InstantaneousAction("ship-order", o=Order)
    o = ship_order.parameter("o")
    p_var = Variable("p", Product)
    ship_order.add_precondition(started(o))
    ship_order.add_precondition(
        Forall(Implies(includes(o, p_var), made(p_var)), p_var))
    ship_order.add_precondition(LT(stacks_avail, Int(3)))
    ship_order.add_effect(started(o),   False)
    ship_order.add_effect(shipped(o),   True)
    ship_order.add_effect(stacks_avail, Plus(stacks_avail, Int(1)))
    p.add_action(ship_order)

    # open-new-stack: stacks-avail < 3
    open_stack = InstantaneousAction("open-new-stack")
    open_stack.add_precondition(LT(stacks_avail, Int(3)))
    open_stack.add_effect(stacks_avail, Plus(stacks_avail, Int(1)))
    p.add_action(open_stack)

    p.add_goal(shipped(o1))
    p.add_goal(shipped(o2))
    return p
