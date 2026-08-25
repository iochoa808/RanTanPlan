;; PDDL-XTS translation of pddl/test/counters (fn-counters).
;; FEATURE: bounded integers.
;;   The original models each counter's (value ?c) as an unbounded numeric
;;   fluent with a separate static (max_int) cap.  In PDDL-XTS the cap becomes
;;   the value type's upper bound: (value ?c) : (number 0 4).  The explicit
;;   max_int fluent and the (<= (+ (value ?c) 1) (max_int)) precondition are
;;   replaced by a (< (value ?c) 4) guard against overflow of the bounded type.

(define (domain fn-counters-xts)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        counter - object
        cval    - (number 0 4)    ;; was the static (max_int) = 4
    )

    (:functions
        (value ?c - counter) - cval
    )

    (:action increment
        :parameters (?c - counter)
        :precondition (< (value ?c) 4)
        :effect (increase (value ?c) 1)
    )

    (:action decrement
        :parameters (?c - counter)
        :precondition (> (value ?c) 0)
        :effect (decrease (value ?c) 1)
    )
)
