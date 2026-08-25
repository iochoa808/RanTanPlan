"""
Shelves showcase — multiple set fluents with all set operations.
PDDL-XTS: sets2
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('sets_showcase')

    Item = UserType('Item')
    a, b, c, d, e, f = [Object(n, Item) for n in ('a','b','c','d','e','f')]
    p.add_objects([a, b, c, d, e, f])

    shelf1 = Fluent('shelf1', SetType(Item))
    shelf2 = Fluent('shelf2', SetType(Item))
    shelf3 = Fluent('shelf3', SetType(Item))
    p.add_fluent(shelf1, default_initial_value=set())
    p.add_fluent(shelf2, default_initial_value=set())
    p.add_fluent(shelf3, default_initial_value=set())

    p.set_initial_value(shelf1, {a, b, c, f})
    p.set_initial_value(shelf2, {a})
    p.set_initial_value(shelf3, {b, c, d, e})

    add_item = InstantaneousAction('add_item', x=Item)
    x = add_item.parameter('x')
    add_item.add_precondition(Not(SetMember(x, shelf1)))
    add_item.add_effect(shelf1, SetAdd(x, shelf1))
    p.add_action(add_item)

    remove_item = InstantaneousAction('remove_item', x=Item)
    x = remove_item.parameter('x')
    remove_item.add_precondition(SetMember(x, shelf1))
    remove_item.add_effect(shelf1, SetRemove(x, shelf1))
    p.add_action(remove_item)

    bulk_add = InstantaneousAction('bulk_add')
    bulk_add.add_precondition(SetDisjoint(shelf1, shelf3))
    bulk_add.add_effect(shelf1, SetUnion(shelf1, shelf3))
    p.add_action(bulk_add)

    common_only = InstantaneousAction('common_only')
    common_only.add_precondition(GE(SetCardinality(shelf1), 2))
    common_only.add_effect(shelf1, SetIntersection(shelf1, shelf3))
    p.add_action(common_only)

    complement = InstantaneousAction('complement')
    complement.add_precondition(SetSubseteq(shelf1, shelf3))
    complement.add_effect(shelf1, SetDifference(shelf3, shelf1))
    p.add_action(complement)

    bulk_fill = InstantaneousAction('bulk_fill')
    bulk_fill.add_precondition(SetDisjoint(shelf2, shelf3))
    bulk_fill.add_effect(shelf2, SetUnion(shelf2, shelf3))
    p.add_action(bulk_fill)

    p.add_goal(Equals(shelf1, {a, d, e, f}))
    p.add_goal(Equals(shelf2, {a, b, c, d, e}))
    return p
