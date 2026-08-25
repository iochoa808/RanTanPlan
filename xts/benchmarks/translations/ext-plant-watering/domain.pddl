;; PDDL-XTS translation of pddl/test/ext-plant-watering.
;; FEATURE: bounded integers.
;;   Extends plant-watering with a per-agent (max_carry) cap and a global
;;   (water_reserve).  Coordinates -> (number 1 4); water counters -> (number 0 25);
;;   per-agent carry -> (number 0 5).  Static maxx/minx/maxy/miny fold into the
;;   coordinate bound; the move guards become (< (x ?a) 4)/(> (x ?a) 1) etc.

(define (domain ext-plant-watering-xts)
    (:requirements :typing :numeric-fluents :bounded-integers)
    (:types
        thing location - object
        agent plant tap - thing
        coord - (number 1 4)
        carry - (number 0 5)
        water - (number 0 25)
    )
    (:functions
        (max_carry ?a - agent) - carry
        (water_reserve) - water
        (x ?t - thing) - coord
        (y ?t - thing) - coord
        (carrying ?a - agent) - carry
        (poured ?p - plant) - water
        (total_poured) - water
        (total_loaded) - water
    )

    (:action move_up :parameters (?a - agent)
        :precondition (< (y ?a) 4) :effect (increase (y ?a) 1))
    (:action move_down :parameters (?a - agent)
        :precondition (> (y ?a) 1) :effect (decrease (y ?a) 1))
    (:action move_right :parameters (?a - agent)
        :precondition (< (x ?a) 4) :effect (increase (x ?a) 1))
    (:action move_left :parameters (?a - agent)
        :precondition (> (x ?a) 1) :effect (decrease (x ?a) 1))
    (:action move_up_left :parameters (?a - agent)
        :precondition (and (> (x ?a) 1) (< (y ?a) 4))
        :effect (and (increase (y ?a) 1) (decrease (x ?a) 1)))
    (:action move_up_right :parameters (?a - agent)
        :precondition (and (< (x ?a) 4) (< (y ?a) 4))
        :effect (and (increase (y ?a) 1) (increase (x ?a) 1)))
    (:action move_down_left :parameters (?a - agent)
        :precondition (and (> (x ?a) 1) (> (y ?a) 1))
        :effect (and (decrease (x ?a) 1) (decrease (y ?a) 1)))
    (:action move_down_right :parameters (?a - agent)
        :precondition (and (< (x ?a) 4) (> (y ?a) 1))
        :effect (and (decrease (y ?a) 1) (increase (x ?a) 1)))

    (:action load :parameters (?a - agent ?t - tap)
        :precondition (and (= (x ?a) (x ?t)) (= (y ?a) (y ?t))
                           (< (carrying ?a) (max_carry ?a)) (>= (water_reserve) 1))
        :effect (and (decrease (water_reserve) 1) (increase (carrying ?a) 1)
                     (increase (total_loaded) 1)))
    (:action pour :parameters (?a - agent ?p - plant)
        :precondition (and (= (x ?a) (x ?p)) (= (y ?a) (y ?p)) (>= (carrying ?a) 1))
        :effect (and (decrease (carrying ?a) 1) (increase (poured ?p) 1)
                     (increase (total_poured) 1)))
)
