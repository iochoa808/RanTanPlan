;; basket = {}.  Goal: basket = {item_a, item_b}.
;; Plan: add(item_a), add(item_b)  — 2 steps.

(define (problem basket-01)
    (:domain basket)

    (:init
        (= (basket) (set.mk ()))
    )

    (:goal (= (basket) (set.mk (item_a item_b))))
)
