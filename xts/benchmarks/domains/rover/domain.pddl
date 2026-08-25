;; Adapted from small-test/rover/domain.pddl
;; PDDL+ changes:
;;   1. bounded-integers: energy in [0, 100] (removes unbounded numeric fluent).
;;   2. sets: four binary predicates replaced by set fluents.
;;        have_soil_analysis ?r ?w  →  (soil-analyzed ?r) - wayptset
;;        have_rock_analysis ?r ?w  →  (rock-analyzed ?r) - wayptset
;;        communicated_soil_data ?w →  (communicated-soil) - wayptset
;;        communicated_rock_data ?w →  (communicated-rock) - wayptset
;;      sample_soil/sample_rock now add to a per-rover set instead of asserting
;;      a predicate.  communicate_soil/rock_data use member as the precondition
;;      and add to a global set as the effect.
;;   have_image and communicated_image_data are kept as predicates because they
;;   have two non-rover arguments (objective × mode), making a clean set encoding
;;   harder without a product type.

(define (domain rover)
    (:requirements :typing :numeric-fluents :bounded-integers :sets)

    (:types
        rover     - object
        waypoint  - object
        store     - object
        camera    - object
        mode      - object
        lander    - object
        objective - object

        energy-val - (number 0 100)
        wayptset   - (set waypoint)
    )

    (:predicates
        (in ?x - rover ?y - waypoint)
        (at_lander ?x - lander ?y - waypoint)
        (can_traverse ?r - rover ?x - waypoint ?y - waypoint)
        (equipped_for_soil_analysis ?r - rover)
        (equipped_for_rock_analysis ?r - rover)
        (equipped_for_imaging ?r - rover)
        (empty ?s - store)
        (full ?s - store)
        (calibrated ?c - camera ?r - rover)
        (supports ?c - camera ?m - mode)
        (available ?r - rover)
        (visible ?w - waypoint ?p - waypoint)
        (have_image ?r - rover ?o - objective ?m - mode)
        (communicated_image_data ?o - objective ?m - mode)
        (at_soil_sample ?w - waypoint)
        (at_rock_sample ?w - waypoint)
        (visible_from ?o - objective ?w - waypoint)
        (store_of ?s - store ?r - rover)
        (calibration_target ?i - camera ?o - objective)
        (on_board ?i - camera ?r - rover)
        (channel_free ?l - lander)
        (in_sun ?w - waypoint)
    )

    (:functions
        (energy ?r - rover)          - energy-val
        (recharges)
        (soil-analyzed ?r - rover)   - wayptset
        (rock-analyzed ?r - rover)   - wayptset
        (communicated-soil)          - wayptset
        (communicated-rock)          - wayptset
    )

    (:action navigate
        :parameters (?x - rover ?y - waypoint ?z - waypoint)
        :precondition (and (can_traverse ?x ?y ?z) (available ?x) (in ?x ?y)
                           (visible ?y ?z) (>= (energy ?x) 8))
        :effect (and (decrease (energy ?x) 8) (not (in ?x ?y)) (in ?x ?z))
    )

    (:action recharge
        :parameters (?x - rover ?w - waypoint)
        :precondition (and (in ?x ?w) (in_sun ?w) (<= (energy ?x) 80))
        :effect (and (increase (energy ?x) 20) (increase (recharges) 1))
    )

    (:action sample_soil
        :parameters (?x - rover ?s - store ?p - waypoint)
        :precondition (and (in ?x ?p) (>= (energy ?x) 3) (at_soil_sample ?p)
                           (equipped_for_soil_analysis ?x) (store_of ?s ?x) (empty ?s))
        :effect (and (not (empty ?s)) (full ?s) (decrease (energy ?x) 3)
                     (add ?p (soil-analyzed ?x)) (not (at_soil_sample ?p)))
    )

    (:action sample_rock
        :parameters (?x - rover ?s - store ?p - waypoint)
        :precondition (and (in ?x ?p) (>= (energy ?x) 5) (at_rock_sample ?p)
                           (equipped_for_rock_analysis ?x) (store_of ?s ?x) (empty ?s))
        :effect (and (not (empty ?s)) (full ?s) (decrease (energy ?x) 5)
                     (add ?p (rock-analyzed ?x)) (not (at_rock_sample ?p)))
    )

    (:action drop
        :parameters (?x - rover ?y - store)
        :precondition (and (store_of ?y ?x) (full ?y))
        :effect (and (not (full ?y)) (empty ?y))
    )

    (:action calibrate
        :parameters (?r - rover ?i - camera ?t - objective ?w - waypoint)
        :precondition (and (equipped_for_imaging ?r) (>= (energy ?r) 2)
                           (calibration_target ?i ?t) (in ?r ?w)
                           (visible_from ?t ?w) (on_board ?i ?r))
        :effect (and (decrease (energy ?r) 2) (calibrated ?i ?r))
    )

    (:action take_image
        :parameters (?r - rover ?p - waypoint ?o - objective ?i - camera ?m - mode)
        :precondition (and (calibrated ?i ?r) (on_board ?i ?r) (equipped_for_imaging ?r)
                           (supports ?i ?m) (visible_from ?o ?p) (in ?r ?p)
                           (>= (energy ?r) 1))
        :effect (and (have_image ?r ?o ?m) (not (calibrated ?i ?r)) (decrease (energy ?r) 1))
    )

    (:action communicate_soil_data
        :parameters (?r - rover ?l - lander ?p - waypoint ?x - waypoint ?y - waypoint)
        :precondition (and (in ?r ?x) (at_lander ?l ?y)
                           (member ?p (soil-analyzed ?r))
                           (visible ?x ?y) (available ?r) (channel_free ?l)
                           (>= (energy ?r) 4))
        :effect (and (add ?p (communicated-soil)) (available ?r) (decrease (energy ?r) 4))
    )

    (:action communicate_rock_data
        :parameters (?r - rover ?l - lander ?p - waypoint ?x - waypoint ?y - waypoint)
        :precondition (and (in ?r ?x) (at_lander ?l ?y)
                           (member ?p (rock-analyzed ?r))
                           (visible ?x ?y) (available ?r) (channel_free ?l)
                           (>= (energy ?r) 4))
        :effect (and (add ?p (communicated-rock)) (available ?r) (decrease (energy ?r) 4))
    )

    (:action communicate_image_data
        :parameters (?r - rover ?l - lander ?o - objective ?m - mode ?x - waypoint ?y - waypoint)
        :precondition (and (in ?r ?x) (at_lander ?l ?y) (have_image ?r ?o ?m)
                           (visible ?x ?y) (available ?r) (channel_free ?l)
                           (>= (energy ?r) 6))
        :effect (and (communicated_image_data ?o ?m) (available ?r) (decrease (energy ?r) 6))
    )
)
