;; counter=0.  Goal: counter=4.
;; step-t = (number 2 2): only valid step is 2.
;; Plan: boost(2), boost(2)  — 2 steps.

(define (problem single-value-01)
    (:domain single-value)

    (:init
        (= (counter) 0)
    )

    (:goal (= (counter) 4))
)
