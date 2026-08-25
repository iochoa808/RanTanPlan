;; counter = 1.  Goal: counter = 7.
;; Plan: boost(3), boost(3)  — 2 steps  (1+3+3=7).

(define (problem step-counter-01)
    (:domain step-counter)

    (:init
        (= (counter) 1)
    )

    (:goal (= (counter) 7))
)
