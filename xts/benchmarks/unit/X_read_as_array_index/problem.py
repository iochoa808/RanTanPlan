"""Using an array read result as the index into another array (indirect indexing). PDDL-XTS: X_read_as_array_index"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *

def get_problem():
    p = Problem('x_read_as_array_index')

    ptr_t = IntType(0, 2)
    val_t = IntType(0, 9)

    pointers = Fluent('pointers', ArrayType(3, ptr_t))
    data     = Fluent('data',     ArrayType(3, val_t))
    result   = Fluent('result',   val_t)
    p.add_fluent(pointers)
    p.add_fluent(data)
    p.add_fluent(result, default_initial_value=0)
    p.set_initial_value(pointers, [2, 0, 1])
    p.set_initial_value(data,     [7, 3, 5])

    # Error: data[pointers[i]] uses an array read expression as an array index
    indirect_read = InstantaneousAction('indirect_read', i=ptr_t)
    i = indirect_read.parameter('i')
    indirect_read.add_effect(result, data[pointers[i]])
    p.add_action(indirect_read)

    p.add_goal(Equals(result, 5))
    return p