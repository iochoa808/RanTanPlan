;; PDDL-XTS translation of pddl/test/plant-watering.
;; FEATURE: bounded integers.
;;   - agent/plant/tap x,y coordinates -> (number 1 4) (the static maxx/minx/
;;     maxy/miny = 1..4 fold into the coordinate type).
;;   - carrying / poured / total_poured / total_loaded -> bounded ints (number 0 20)
;;     (the static max_int = 80 cap shrinks to a value sufficient for this
;;     instance, which only pours 4 units).
;;   move guards (<= (+ (y ?a) 1) (maxy)) become (< (y ?a) 4), etc.
;;   Cost metric dropped (satisficing).

(define (domain plant-watering-xts)
    (:requirements :typing :numeric-fluents :bounded-integers)
    (:types
        thing location - object
        agent plant tap - thing
        coord - (number 1 4)
        water - (number 0 20)
    )
    (:functions
        (x ?t - thing) - coord
        (y ?t - thing) - coord
        (carrying) - water
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
                           (< (total_loaded) 20) (< (carrying) 20))
        :effect (and (increase (carrying) 1) (increase (total_loaded) 1)))
    (:action pour :parameters (?a - agent ?p - plant)
        :precondition (and (= (x ?a) (x ?p)) (= (y ?a) (y ?p))
                           (>= (carrying) 1) (< (total_poured) 20) (< (poured ?p) 20))
        :effect (and (decrease (carrying) 1) (increase (poured ?p) 1)
                     (increase (total_poured) 1)))
)
