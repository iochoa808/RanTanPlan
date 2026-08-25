;; PDDL-XTS translation of pddl/test/drone.
;; FEATURE: bounded integers.
;;   3D UAV position (x)/(y)/(z) -> bounded ints whose ranges fold the static
;;   min_*/max_* bounds; battery-level -> a bounded int.  Per-location target
;;   coordinates (xl/yl/zl) stay (static) bounded ints.

(define (domain drone-xts)
    (:requirements :typing :numeric-fluents :bounded-integers)
    (:types
        location - object
        cx - (number 0 1)
        cy - (number 0 1)
        cz - (number 0 2)
        batt - (number 0 9)
    )
    (:predicates (visited ?x - location))
    (:functions
        (x) - cx
        (y) - cy
        (z) - cz
        (xl ?l - location) - cx
        (yl ?l - location) - cy
        (zl ?l - location) - cz
        (battery-level) - batt
        (battery-level-full) - batt
    )

    (:action increase_x :parameters ()
        :precondition (and (>= (battery-level) 1) (< (x) 1))
        :effect (and (increase (x) 1) (decrease (battery-level) 1)))
    (:action decrease_x :parameters ()
        :precondition (and (>= (battery-level) 1) (> (x) 0))
        :effect (and (decrease (x) 1) (decrease (battery-level) 1)))
    (:action increase_y :parameters ()
        :precondition (and (>= (battery-level) 1) (< (y) 1))
        :effect (and (increase (y) 1) (decrease (battery-level) 1)))
    (:action decrease_y :parameters ()
        :precondition (and (>= (battery-level) 1) (> (y) 0))
        :effect (and (decrease (y) 1) (decrease (battery-level) 1)))
    (:action increase_z :parameters ()
        :precondition (and (>= (battery-level) 1) (< (z) 2))
        :effect (and (increase (z) 1) (decrease (battery-level) 1)))
    (:action decrease_z :parameters ()
        :precondition (and (>= (battery-level) 1) (> (z) 0))
        :effect (and (decrease (z) 1) (decrease (battery-level) 1)))

    (:action visit :parameters (?l - location)
        :precondition (and (>= (battery-level) 1)
                           (= (xl ?l) (x)) (= (yl ?l) (y)) (= (zl ?l) (z)))
        :effect (and (visited ?l) (decrease (battery-level) 1)))
    (:action recharge :parameters ()
        :precondition (and (= (x) 0) (= (y) 0) (= (z) 0))
        :effect (assign (battery-level) (battery-level-full)))
)
