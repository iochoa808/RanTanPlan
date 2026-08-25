;; basket = {item_c, item_d, item_a} (3 items, reset fires immediately).
;; After reset: basket = {item_a, item_b}.
;; Goal: item_a ∈ basket AND item_b ∈ basket AND item_c ∉ basket.
;;
;; Plan: reset_to_ab()  — 1 step.

(define (problem reset-basket-01)
    (:domain reset-basket)

    (:init
        (= (basket) (set.mk (item_c item_d item_a)))
    )

    (:goal (and
        (member item_a (basket))
        (member item_b (basket))
        (not (member item_c (basket)))
    ))
)
