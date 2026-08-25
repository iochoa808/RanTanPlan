;; Adapted from small-test/drone/domain.pddl for the p3 instance (1×8×1 grid).
;; PDDL+ change: bounded integers for all three coordinates and for battery.
;; Seven static fluents are eliminated: min_x, max_x, min_y, max_y, min_z, max_z,
;; battery-level-full.  All their appearances in preconditions become literals,
;; and recharge assigns the literal 21 instead of (battery-level-full).

(define (domain drone)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        location - object

        coord-x - (number 0 1)   ; x in [0, 1]
        coord-y - (number 0 8)   ; y in [0, 8]
        coord-z - (number 0 1)   ; z in [0, 1]
        battery - (number 0 21)  ; charge level
    )

    (:predicates
        (visited ?l - location)
    )

    (:functions
        (x)             - coord-x
        (y)             - coord-y
        (z)             - coord-z
        (battery-level) - battery
        (xl ?l - location)
        (yl ?l - location)
        (zl ?l - location)
    )

    (:action increase_x
        :parameters ()
        :precondition (and (>= (battery-level) 1) (< (x) 1))
        :effect (and (increase (x) 1) (decrease (battery-level) 1))
    )

    (:action decrease_x
        :parameters ()
        :precondition (and (>= (battery-level) 1) (> (x) 0))
        :effect (and (decrease (x) 1) (decrease (battery-level) 1))
    )

    (:action increase_y
        :parameters ()
        :precondition (and (>= (battery-level) 1) (< (y) 8))
        :effect (and (increase (y) 1) (decrease (battery-level) 1))
    )

    (:action decrease_y
        :parameters ()
        :precondition (and (>= (battery-level) 1) (> (y) 0))
        :effect (and (decrease (y) 1) (decrease (battery-level) 1))
    )

    (:action increase_z
        :parameters ()
        :precondition (and (>= (battery-level) 1) (< (z) 1))
        :effect (and (increase (z) 1) (decrease (battery-level) 1))
    )

    (:action decrease_z
        :parameters ()
        :precondition (and (>= (battery-level) 1) (> (z) 0))
        :effect (and (decrease (z) 1) (decrease (battery-level) 1))
    )

    (:action visit
        :parameters (?l - location)
        :precondition (and
            (>= (battery-level) 1)
            (= (xl ?l) (x))
            (= (yl ?l) (y))
            (= (zl ?l) (z))
        )
        :effect (and (visited ?l) (decrease (battery-level) 1))
    )

    (:action recharge
        :parameters ()
        :precondition (and (= (x) 0) (= (y) 0) (= (z) 0))
        :effect (assign (battery-level) 21)
    )
)
