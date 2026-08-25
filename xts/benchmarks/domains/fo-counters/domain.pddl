;; Adapted from small-test/fo-counters/domain.pddl
;; PDDL+ change: bounded integers for value [0,36] and rate [0,10].
;; The max_int fluent is removed entirely — it was only needed to carry
;; the type bound, which is now expressed directly in counter-val.
;; The increase_rate precondition simplifies from
;;   (<= (+ (rate_value ?c) 1) 10)  to  (< (rate_value ?c) 10).

(define (domain fn-counters)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        counter - object            ; counter objects (c0, c1, ...)
        counter-val - (number 0 36) ; bounded integer type for counter values
        rate-val    - (number 0 10) ; bounded integer type for rate values
    )

    (:functions
        (value ?c - counter)      - counter-val
        (rate_value ?c - counter) - rate-val
        (total-cost)
    )

    (:action increment
         :parameters (?c - counter)
         :precondition (<= (+ (value ?c) (rate_value ?c)) 36)
         :effect (and (increase (value ?c) (rate_value ?c)) (increase (total-cost) 1))
    )

    (:action decrement
         :parameters (?c - counter)
         :precondition (>= (- (value ?c) (rate_value ?c)) 0)
         :effect (and (decrease (value ?c) (rate_value ?c)) (increase (total-cost) 1))
    )

    (:action increase_rate
         :parameters (?c - counter)
         :precondition (< (rate_value ?c) 10)
         :effect (and (increase (rate_value ?c) 1) (increase (total-cost) 1))
    )

    (:action decrement_rate
         :parameters (?c - counter)
         :precondition (> (rate_value ?c) 0)
         :effect (and (decrease (rate_value ?c) 1) (increase (total-cost) 1))
    )
)
