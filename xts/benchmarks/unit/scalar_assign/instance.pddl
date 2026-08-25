;; a=0, b=7.  Goal: a=7.
;; Plan: copy()  — 1 step.

(define (problem scalar-copy-01)
    (:domain scalar-copy)

    (:init
        (= (a) 0)
        (= (b) 7)
    )

    (:goal (= (a) 7))
)
