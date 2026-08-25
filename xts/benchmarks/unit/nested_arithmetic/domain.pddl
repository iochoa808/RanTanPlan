;; Test: NESTED ARITHMETIC — multi-level arithmetic expressions in effects.
;;
;; KEY PATTERNS:
;;   (assign (result) (+ (* (a) (b)) (c)))   — (a*b)+c (two levels deep)
;;   (assign (result) (* (a) (+ (b) 1)))     — a*(b+1) (one level inside mul)
;;
;; Only flat one-level arithmetic has been tested elsewhere.
;; This verifies that the expression-tree encoder handles deeper nesting.
;;
;; Initial: a=2, b=3, c=4, result=0.
;; Action compute1 produces: 2*3+4 = 10.
;; Goal: result = 10.  Plan: compute1()  — 1 step.

(define (domain nested-arith)
    (:requirements :typing :numeric-fluents :bounded-integers)

    (:types
        small  - (number 0 5)
        result - (number 0 30)
    )

    (:functions
        (a)      - small
        (b)      - small
        (c)      - small
        (result) - result
    )

    ;; result = (a * b) + c
    (:action compute1
        :parameters ()
        :precondition (= (result) 0)
        :effect (assign (result) (+ (* (a) (b)) (c)))
    )

    ;; result = a * (b + 1)
    (:action compute2
        :parameters ()
        :precondition (= (result) 0)
        :effect (assign (result) (* (a) (+ (b) 1)))
    )
)
