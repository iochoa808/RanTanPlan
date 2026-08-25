;; bag = {1,2,3}.  Goal: bag = set.mk ()  (empty).
;; Plan: remove_elem(1), remove_elem(2), remove_elem(3)  — 3 steps.

(define (problem drain-01)
    (:domain drain)

    (:init
        (= (bag) (set.mk (1 2 3)))
    )

    (:goal (= (bag) (set.mk ())))
)
