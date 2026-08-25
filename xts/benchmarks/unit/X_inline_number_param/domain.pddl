;; BREAK TARGET: INLINE (number lo hi) used directly in :parameters without
;;               a named type in :types.
;;
;; The guide requires bounded-integer types to be declared with a name.
;; Writing (:parameters (?v - (number 0 5))) uses an anonymous inline type,
;; which is not allowed — ?v's type must reference a named type.
;;
;; Expected: parse/semantic error — inline anonymous type in :parameters.

(define (domain X-inline-number-param)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        count-t - (number 0 10)
    )

    (:functions
        (counter) - count-t
    )

    ;; Error: ?v typed inline as (number 0 5) instead of referencing a named type.
    (:action inc
        :parameters (?v - (number 0 5))
        :precondition (<= (+ (counter) ?v) 10)
        :effect (assign (counter) (+ (counter) ?v))
    )
)
