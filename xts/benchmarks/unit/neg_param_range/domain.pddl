;; Test: action parameter with a NEGATIVE lower bound: (number -3 3).
;; IPAR must enumerate values -3, -2, -1, 0, 1, 2, 3 and ground set(-2) correctly.
;;
;; result ∈ [-3, 3]; action set(?v) assigns result := ?v; goal result = -2.

(define (domain neg-param-range)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        val - (number -3 3)
    )

    (:functions
        (result) - val
    )

    (:action set
        :parameters (?v - val)
        :precondition ()
        :effect (assign (result) ?v)
    )
)