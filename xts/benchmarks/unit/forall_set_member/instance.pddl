;; bag={0,2,4}.  Goal: verified.
;; Plan: fill(1), fill(3), verify  — 3 steps.

(define (problem full-bag-01)
    (:domain full-bag)

    (:init
        (= (bag) (set.mk (0 2 4)))
    )

    (:goal (verified))
)
