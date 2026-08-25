;; Test: (assign (f1) (f2)) where both are PLAIN SCALAR bounded-integer fluents
;;       (not arrays, not sets — a fluent-to-fluent copy for scalars).
;;
;; KEY PATTERN: (assign (a) (b)) — direct copy between two scalar fluents.
;;
;; Domain: two score fluents; copy one into the other, then raise the target.
;; Initial: a=0, b=7; goal: a=7.
;; Plan: copy()  — 1 step.

(define (domain scalar-copy)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        score - (number 0 9)
    )

    (:functions
        (a) - score
        (b) - score
    )

    ;; Copy b into a.
    (:action copy
        :parameters ()
        :precondition (< (a) (b))
        :effect (assign (a) (b))
    )
)
