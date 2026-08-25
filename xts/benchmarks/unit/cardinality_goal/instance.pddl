;; bag={1}.  Goal: cardinality(bag) = 3  (equality form in goal).
;; Plan: fill(2), fill(3)  — 2 steps.

(define (problem exact-bag-01)
    (:domain exact-bag)

    (:init
        (= (bag) (set.mk (1)))
    )

    (:goal (= (cardinality (bag)) 3))
)
