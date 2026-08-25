;; PDDL-XTS translation of pddl/test/fo-counters (fn-counters, variable-rate).
;; FEATURE: bounded integers.
;;   (value ?c)      : (number 0 4)   ;; was capped by static (max_int)=4
;;   (rate_value ?c) : (number 0 10)  ;; rate cap was the literal 10 in the guard
;;   (total-cost)    : (number 0 50)  ;; bounded so the cost metric stays expressible
;;   The (max_int) fluent and (<= ... (max_int)) guards become explicit bounded-int
;;   range guards against overflow.

(define (domain fn-counters-fo-xts)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        counter - object
        cval    - (number 0 4)
        crate   - (number 0 10)
        ccost   - (number 0 50)
    )

    (:functions
        (value ?c - counter)      - cval
        (rate_value ?c - counter) - crate
        (total-cost)              - ccost
    )

    (:action increment
        :parameters (?c - counter)
        :precondition (<= (+ (value ?c) (rate_value ?c)) 4)
        :effect (and (increase (value ?c) (rate_value ?c)) (increase (total-cost) 1))
    )

    (:action decrement
        :parameters (?c - counter)
        :precondition (>= (- (value ?c) (rate_value ?c)) 0)
        :effect (and (decrease (value ?c) (rate_value ?c)) (increase (total-cost) 1))
    )

    (:action increase_rate
        :parameters (?c - counter)
        :precondition (<= (+ (rate_value ?c) 1) 10)
        :effect (and (increase (rate_value ?c) 1) (increase (total-cost) 1))
    )

    (:action decrement_rate
        :parameters (?c - counter)
        :precondition (>= (rate_value ?c) 1)
        :effect (and (decrease (rate_value ?c) 1) (increase (total-cost) 1))
    )
)
