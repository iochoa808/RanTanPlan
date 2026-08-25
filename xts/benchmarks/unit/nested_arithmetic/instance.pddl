;; a=2, b=3, c=4, result=0.  Goal: result=10.
;; compute1: result = (2*3)+4 = 10.  Plan: compute1()  — 1 step.

(define (problem nested-arith-01)
    (:domain nested-arith)

    (:init
        (= (a) 2)
        (= (b) 3)
        (= (c) 4)
        (= (result) 0)
    )

    (:goal (= (result) 10))
)
