;; BREAK TARGET: initialising a bounded-integer fluent to a value outside
;; the declared [lo, hi] range.
;;
;; `temperature` is declared as (number 0 5), so valid values are 0..5.
;; The problem file initialises it to 99, and `setpoint` is initialised to -3.
;;
;; Out-of-range initialisation should be caught as a semantic error rather
;; than silently clamping, wrapping, or accepting the invalid state.
;;
;; Expected: error for each out-of-range init assignment.

(define (domain X-bounded-init-overflow)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        temp - (number 0 5)
        room - object
    )

    (:functions
        (temperature ?r - room) - temp
    )

    (:action heat
        :parameters (?r - room)
        :precondition (< (temperature ?r) 5)
        :effect (assign (temperature ?r) (+ (temperature ?r) 1))
    )
)
