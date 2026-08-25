;; reachable: only [0][1][0] = 1 (all others 0).
;; Initial: d=0, r=0, c=0.  Goal: r=1.
;; Plan: move(0,1,0)  — 1 step.

(define (problem static-3d-01)
    (:domain static-3d)

    (:init
        (= (reachable) (array.mk (((0 0)
                                    (1 0))
                                   ((0 0)
                                    (0 0)))))
        (= (d) 0)
        (= (r) 0)
        (= (c) 0)
    )

    (:goal (= (r) 1))
)
