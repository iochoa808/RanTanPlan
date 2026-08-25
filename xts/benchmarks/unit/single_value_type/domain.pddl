;; Test: SINGLE-VALUE TYPE — (number k k) where lo = hi = k.
;;
;; KEY PATTERN: a type whose range is a single integer.
;;   (number 2 2) means the only valid value is 2.
;;   Used as an action parameter: every grounding assigns the constant value.
;;
;; Domain: counter [0,10].  step-t = (number 2 2) — always 2.
;;   boost(?s): counter += ?s  (? s is always 2)
;; Initial counter=0; goal: counter=4.
;; Plan: boost(2), boost(2)  — 2 steps  (0+2+2=4).

(define (domain single-value)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        count-t - (number 0 10)
        step-t  - (number 2 2)    ;; constant: only value is 2
    )

    (:functions
        (counter) - count-t
    )

    (:action boost
        :parameters (?s - step-t)
        :precondition (<= (+ (counter) ?s) 10)
        :effect (assign (counter) (+ (counter) ?s))
    )
)
