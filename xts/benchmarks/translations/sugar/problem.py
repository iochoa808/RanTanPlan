"""
Sugar XTS — simple-supply-chain.
Features: object fluent (loader-at), set fluent (connects), bounded ints.
Plan: produce_sugar_max -> setting-machine -> drive_truck -> unload_truck x2  (~5 steps).
"""
import sys, os
sys.path.insert(0, os.path.expanduser("~/unified-planning"))
from unified_planning.shortcuts import *


def get_problem():
    p = Problem('simple-supply-chain-xts')

    Sugar    = UserType('sugar')
    Location = UserType('location')
    Loader   = UserType('loader')
    Brand    = UserType('brand',    father=Sugar)
    RawCane  = UserType('raw-cane', father=Sugar)
    Mill     = UserType('mill',     father=Location)
    Depot    = UserType('depot',    father=Location)
    Truck    = UserType('truck',    father=Loader)
    Crane    = UserType('crane',    father=Loader)
    Farm     = UserType('farm')
    Field    = UserType('field')

    res_t = IntType(0, 15)
    stk_t = IntType(0, 10)
    cap_t = IntType(0, 5)
    chg_t = IntType(0, 2)
    LocSet = SetType(Location)

    # Objects
    brand1    = Object('brand1',    Brand)
    brand2    = Object('brand2',    Brand)
    sugar_cane= Object('sugar-cane', RawCane)
    truck1    = Object('truck1',    Truck)
    depot1    = Object('depot1',    Depot)
    mill1     = Object('mill1',     Mill)
    crane1    = Object('crane1',    Crane)

    p.add_objects([brand1, brand2, sugar_cane, truck1, depot1, mill1, crane1])

    # Fluents
    loader_at      = Fluent('loader-at',       Location, d=Loader)
    connects       = Fluent('connects',         LocSet,   l=Location)
    has_resource   = Fluent('has-resource',     res_t,    r=RawCane, m=Mill)
    max_produce    = Fluent('max-produce',      cap_t,    m=Mill)
    max_changing   = Fluent('max-changing',     chg_t,    m=Mill)
    in_storage     = Fluent('in-storage',       stk_t,    l=Location, b=Brand)
    truck_cap      = Fluent('truck-cap',        stk_t,    t=Truck)
    in_truck_sugar = Fluent('in-truck-sugar',   stk_t,    b=Brand, t=Truck)
    capacity       = Fluent('capacity',         cap_t,    c=Crane)
    service_time   = Fluent('service-time',     cap_t,    c=Crane)
    max_service_time = Fluent('max-service-time', cap_t,  c=Crane)
    cost_process   = Fluent('cost-process',     cap_t,    m=Mill)
    unharvest_field= Fluent('unharvest-field',  chg_t)

    available_m    = Fluent('available',        m=Mill)
    produce_f      = Fluent('produce',          m=Mill, b=Brand)
    current_process= Fluent('current-process',  m=Mill, b=Brand)
    change_process = Fluent('change-process',   b1=Brand, b2=Brand)
    place_order    = Fluent('place-order',      r=RawCane, m=Mill)
    transport_to   = Fluent('transport-to',     r=RawCane, m=Mill)
    busy_m         = Fluent('busy',             m=Mill)
    ready_crane    = Fluent('ready-crane',      c=Crane)
    service_crane  = Fluent('service-crane',    c=Crane)

    p.add_fluent(loader_at,        default_initial_value=mill1)
    p.add_fluent(connects,         default_initial_value=set())
    p.add_fluent(has_resource,     default_initial_value=Int(0))
    p.add_fluent(max_produce,      default_initial_value=Int(0))
    p.add_fluent(max_changing,     default_initial_value=Int(0))
    p.add_fluent(in_storage,       default_initial_value=Int(0))
    p.add_fluent(truck_cap,        default_initial_value=Int(0))
    p.add_fluent(in_truck_sugar,   default_initial_value=Int(0))
    p.add_fluent(capacity,         default_initial_value=Int(0))
    p.add_fluent(service_time,     default_initial_value=Int(0))
    p.add_fluent(max_service_time, default_initial_value=Int(0))
    p.add_fluent(cost_process,     default_initial_value=Int(0))
    p.add_fluent(unharvest_field,  default_initial_value=Int(0))
    p.add_fluent(available_m,      default_initial_value=False)
    p.add_fluent(produce_f,        default_initial_value=False)
    p.add_fluent(current_process,  default_initial_value=False)
    p.add_fluent(change_process,   default_initial_value=False)
    p.add_fluent(place_order,      default_initial_value=False)
    p.add_fluent(transport_to,     default_initial_value=False)
    p.add_fluent(busy_m,           default_initial_value=False)
    p.add_fluent(ready_crane,      default_initial_value=False)
    p.add_fluent(service_crane,    default_initial_value=False)

    # Init
    p.set_initial_value(connects(mill1),  {depot1})
    p.set_initial_value(connects(depot1), {mill1})

    p.set_initial_value(loader_at(truck1), mill1)
    p.set_initial_value(loader_at(crane1), mill1)

    p.set_initial_value(available_m(mill1),        True)
    p.set_initial_value(produce_f(mill1, brand1),  True)
    p.set_initial_value(produce_f(mill1, brand2),  True)
    p.set_initial_value(current_process(mill1, brand1), True)
    p.set_initial_value(change_process(brand1, brand2), True)
    p.set_initial_value(change_process(brand2, brand1), True)
    p.set_initial_value(cost_process(mill1),    Int(1))
    p.set_initial_value(max_produce(mill1),     Int(3))
    p.set_initial_value(max_changing(mill1),    Int(2))
    p.set_initial_value(has_resource(sugar_cane, mill1), Int(5))

    p.set_initial_value(in_storage(mill1,  brand1), Int(0))
    p.set_initial_value(in_storage(mill1,  brand2), Int(0))
    p.set_initial_value(in_storage(depot1, brand1), Int(0))
    p.set_initial_value(in_storage(depot1, brand2), Int(0))

    p.set_initial_value(truck_cap(truck1),              Int(5))
    p.set_initial_value(in_truck_sugar(brand1, truck1), Int(0))
    p.set_initial_value(in_truck_sugar(brand2, truck1), Int(0))

    p.set_initial_value(ready_crane(crane1),           True)
    p.set_initial_value(capacity(crane1),              Int(2))
    p.set_initial_value(service_time(crane1),          Int(5))
    p.set_initial_value(max_service_time(crane1),      Int(5))

    p.set_initial_value(unharvest_field, Int(2))

    # ---- Actions ----

    produce_sugar = InstantaneousAction('produce_sugar', r=RawCane, m=Mill, b=Brand)
    r_p, m_p, b_p = [produce_sugar.parameter(x) for x in ('r', 'm', 'b')]
    produce_sugar.add_precondition(current_process(m_p, b_p))
    produce_sugar.add_precondition(available_m(m_p))
    produce_sugar.add_precondition(produce_f(m_p, b_p))
    produce_sugar.add_precondition(GT(has_resource(r_p, m_p), Int(0)))
    produce_sugar.add_precondition(GT(max_changing(m_p), Int(0)))
    produce_sugar.add_effect(in_storage(m_p, b_p), Plus(in_storage(m_p, b_p), Int(1)))
    produce_sugar.add_effect(has_resource(r_p, m_p), Minus(has_resource(r_p, m_p), Int(1)))
    produce_sugar.add_effect(busy_m(m_p),      True)
    produce_sugar.add_effect(available_m(m_p), False)
    p.add_action(produce_sugar)

    produce_sugar_max = InstantaneousAction('produce_sugar_max', r=RawCane, m=Mill, b=Brand)
    r_p, m_p, b_p = [produce_sugar_max.parameter(x) for x in ('r', 'm', 'b')]
    produce_sugar_max.add_precondition(current_process(m_p, b_p))
    produce_sugar_max.add_precondition(available_m(m_p))
    produce_sugar_max.add_precondition(produce_f(m_p, b_p))
    produce_sugar_max.add_precondition(GE(has_resource(r_p, m_p), max_produce(m_p)))
    produce_sugar_max.add_precondition(GT(max_changing(m_p), Int(0)))
    produce_sugar_max.add_effect(in_storage(m_p, b_p),
                                 Plus(in_storage(m_p, b_p), max_produce(m_p)))
    produce_sugar_max.add_effect(has_resource(r_p, m_p),
                                 Minus(has_resource(r_p, m_p), max_produce(m_p)))
    produce_sugar_max.add_effect(busy_m(m_p),      True)
    produce_sugar_max.add_effect(available_m(m_p), False)
    p.add_action(produce_sugar_max)

    order_sugar_cane = InstantaneousAction('order-sugar-cane', r=RawCane, m=Mill)
    r_p, m_p = order_sugar_cane.parameter('r'), order_sugar_cane.parameter('m')
    order_sugar_cane.add_precondition(Equals(has_resource(r_p, m_p), Int(0)))
    order_sugar_cane.add_effect(place_order(r_p, m_p), True)
    p.add_action(order_sugar_cane)

    setting_machine = InstantaneousAction('setting-machine', m=Mill)
    m_p = setting_machine.parameter('m')
    setting_machine.add_precondition(busy_m(m_p))
    setting_machine.add_effect(busy_m(m_p),      False)
    setting_machine.add_effect(available_m(m_p), True)
    p.add_action(setting_machine)

    change_product = InstantaneousAction('change-product', m=Mill, b1=Brand, b2=Brand)
    m_p, b1_p, b2_p = [change_product.parameter(x) for x in ('m', 'b1', 'b2')]
    change_product.add_precondition(current_process(m_p, b1_p))
    change_product.add_precondition(change_process(b1_p, b2_p))
    change_product.add_effect(current_process(m_p, b2_p), True)
    change_product.add_effect(current_process(m_p, b1_p), False)
    change_product.add_effect(max_changing(m_p), Minus(max_changing(m_p), Int(1)))
    p.add_action(change_product)

    sugar_cane_farm = InstantaneousAction('sugar-cane-farm', r=RawCane, m=Mill)
    r_p, m_p = sugar_cane_farm.parameter('r'), sugar_cane_farm.parameter('m')
    sugar_cane_farm.add_precondition(place_order(r_p, m_p))
    sugar_cane_farm.add_precondition(GT(unharvest_field, Int(0)))
    sugar_cane_farm.add_effect(unharvest_field,   Minus(unharvest_field, Int(1)))
    sugar_cane_farm.add_effect(has_resource(r_p, m_p), Plus(has_resource(r_p, m_p), Int(5)))
    sugar_cane_farm.add_effect(place_order(r_p, m_p),  False)
    p.add_action(sugar_cane_farm)

    sugar_cane_mills = InstantaneousAction('sugar-cane-mills', r=RawCane, m1=Mill, m2=Mill)
    r_p, m1_p, m2_p = [sugar_cane_mills.parameter(x) for x in ('r', 'm1', 'm2')]
    sugar_cane_mills.add_precondition(place_order(r_p, m1_p))
    sugar_cane_mills.add_precondition(GT(has_resource(r_p, m2_p), Int(0)))
    sugar_cane_mills.add_effect(has_resource(r_p, m1_p), Plus(has_resource(r_p, m1_p), Int(1)))
    sugar_cane_mills.add_effect(has_resource(r_p, m2_p), Minus(has_resource(r_p, m2_p), Int(1)))
    sugar_cane_mills.add_effect(place_order(r_p, m1_p),  False)
    p.add_action(sugar_cane_mills)

    load_truck_crane = InstantaneousAction('load_truck_crane', b=Brand, t=Truck, l=Location, c=Crane)
    b_p, t_p, l_p, c_p = [load_truck_crane.parameter(x) for x in ('b', 't', 'l', 'c')]
    load_truck_crane.add_precondition(Equals(loader_at(t_p), l_p))
    load_truck_crane.add_precondition(Equals(loader_at(c_p), l_p))
    load_truck_crane.add_precondition(GE(in_storage(l_p, b_p), capacity(c_p)))
    load_truck_crane.add_precondition(GE(truck_cap(t_p),       capacity(c_p)))
    load_truck_crane.add_precondition(ready_crane(c_p))
    load_truck_crane.add_effect(in_storage(l_p, b_p),   Minus(in_storage(l_p, b_p), capacity(c_p)))
    load_truck_crane.add_effect(truck_cap(t_p),          Minus(truck_cap(t_p),       capacity(c_p)))
    load_truck_crane.add_effect(in_truck_sugar(b_p, t_p), Plus(in_truck_sugar(b_p, t_p), capacity(c_p)))
    p.add_action(load_truck_crane)

    check_service = InstantaneousAction('check-service', c=Crane, l=Location)
    c_p, l_p = check_service.parameter('c'), check_service.parameter('l')
    check_service.add_precondition(Equals(loader_at(c_p), l_p))
    check_service.add_precondition(Equals(service_time(c_p), Int(0)))
    check_service.add_effect(ready_crane(c_p),   False)
    check_service.add_effect(service_crane(c_p), True)
    check_service.add_effect(service_time(c_p),  Plus(service_time(c_p), max_service_time(c_p)))
    p.add_action(check_service)

    maintainence_crane = InstantaneousAction('maintainence-crane', c=Crane, l=Location)
    c_p, l_p = maintainence_crane.parameter('c'), maintainence_crane.parameter('l')
    maintainence_crane.add_precondition(Equals(loader_at(c_p), l_p))
    maintainence_crane.add_precondition(service_crane(c_p))
    maintainence_crane.add_effect(ready_crane(c_p), True)
    p.add_action(maintainence_crane)

    load_truck_manual = InstantaneousAction('load-truck-manual', b=Brand, t=Truck, l=Location)
    b_p, t_p, l_p = [load_truck_manual.parameter(x) for x in ('b', 't', 'l')]
    load_truck_manual.add_precondition(Equals(loader_at(t_p), l_p))
    load_truck_manual.add_precondition(GT(in_storage(l_p, b_p), Int(0)))
    load_truck_manual.add_precondition(GT(truck_cap(t_p), Int(0)))
    load_truck_manual.add_effect(in_storage(l_p, b_p),    Minus(in_storage(l_p, b_p), Int(1)))
    load_truck_manual.add_effect(truck_cap(t_p),           Minus(truck_cap(t_p), Int(1)))
    load_truck_manual.add_effect(in_truck_sugar(b_p, t_p), Plus(in_truck_sugar(b_p, t_p), Int(1)))
    p.add_action(load_truck_manual)

    drive_truck = InstantaneousAction('drive_truck', t=Truck, y1=Location, y2=Location)
    t_p, y1_p, y2_p = [drive_truck.parameter(x) for x in ('t', 'y1', 'y2')]
    drive_truck.add_precondition(Equals(loader_at(t_p), y1_p))
    drive_truck.add_precondition(SetMember(y2_p, connects(y1_p)))
    drive_truck.add_effect(loader_at(t_p), y2_p)
    p.add_action(drive_truck)

    unload_truck = InstantaneousAction('unload_truck', b=Brand, t=Truck, l=Location)
    b_p, t_p, l_p = [unload_truck.parameter(x) for x in ('b', 't', 'l')]
    unload_truck.add_precondition(Equals(loader_at(t_p), l_p))
    unload_truck.add_precondition(GT(in_truck_sugar(b_p, t_p), Int(0)))
    unload_truck.add_effect(in_storage(l_p, b_p),    Plus(in_storage(l_p, b_p), Int(1)))
    unload_truck.add_effect(in_truck_sugar(b_p, t_p), Minus(in_truck_sugar(b_p, t_p), Int(1)))
    unload_truck.add_effect(truck_cap(t_p),            Plus(truck_cap(t_p), Int(1)))
    p.add_action(unload_truck)

    # Goal: >= 2 brand1 sugar at depot1
    p.add_goal(GE(in_storage(depot1, brand1), Int(2)))

    return p
