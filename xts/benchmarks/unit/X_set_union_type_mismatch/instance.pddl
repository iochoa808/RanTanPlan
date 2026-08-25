;; The domain is ill-typed; this instance only exists so the planner is invoked.
(define (problem X-set-union-type-mismatch-01)
    (:domain X-set-union-type-mismatch)

    (:objects x y - item)

    (:init
        (= (ob)  (set.mk (x)))
        (= (ib)  (set.mk (1 2)))
        (= (res) (set.mk (x)))
    )

    (:goal (member y (res)))
)
