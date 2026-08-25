;; grid=[[1,1],[1,0]]; done=false.  Goal: done.
;; Plan: set(1,1), verify()  — 2 steps.
;; After set(1,1): grid=[[1,1],[1,1]] → forall forall passes → verify fires.

(define (problem nested-forall-prec-01)
    (:domain nested-forall-prec)

    (:init
        (= (grid) (array.mk (1 1) (1 0)))
    )

    (:goal (done))
)
