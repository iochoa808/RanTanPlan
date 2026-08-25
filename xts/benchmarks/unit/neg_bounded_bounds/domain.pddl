;; Bounded-integer type with a negative lower bound.
;; temp ∈ [-5, 5]; action warm increments t; goal t = 2 from t = -3.

(define (domain neg-bounded-bounds)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        temp - (number -5 5)
    )

    (:functions
        (t) - temp
    )

    (:action warm
        :parameters ()
        :precondition (< (t) 5)
        :effect (increase (t) 1)
    )
)