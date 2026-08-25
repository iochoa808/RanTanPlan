;; arr=[0,5,0]; done=false.  Goal: done.
;; Plan: reset(1), check()  — 2 steps (reset zeros cell 1, then forall guard passes).

(define (problem when-forall-guard-01)
    (:domain forall-when-guard)

    (:init
        (= (cells) (array.mk (0 5 0)))
    )

    (:goal (done))
)
