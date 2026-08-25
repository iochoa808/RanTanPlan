;; Test: (increase (counter) ?step) and (decrease (counter) ?step) with a
;;       BOUNDED INTEGER PARAMETER as the step amount.
;;
;; KEY PATTERNS:
;;   (increase (counter) ?step)  — add ?step to counter
;;   (decrease (counter) ?step)  — subtract ?step from counter
;;
;; All other tests use a constant step of 1. This verifies that a param-valued
;; step works through the bounded-integer encoding unchanged.
;;
;; Initial counter = 1; goal: counter = 7.
;; Plan: boost(3), boost(3)  — 2 steps  (1 + 3 + 3 = 7).

(define (domain step-counter)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        count-t - (number 0 10)
        step-t  - (number 1 4)    ;; increment / decrement amount
    )

    (:functions
        (counter) - count-t
    )

    ;; (increase (counter) ?step) — increase by param
    (:action boost
        :parameters (?step - step-t)
        :precondition (<= (+ (counter) ?step) 10)
        :effect (increase (counter) ?step)
    )

    ;; (decrease (counter) ?step) — decrease by param
    (:action cut
        :parameters (?step - step-t)
        :precondition (>= (- (counter) ?step) 0)
        :effect (decrease (counter) ?step)
    )
)
