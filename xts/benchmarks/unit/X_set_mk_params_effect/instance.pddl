(define (problem X-set-mk-params-effect-01)
    (:domain X-set-mk-params-effect)
    (:init
        (= (basket) (set.mk (item_a item_b item_c)))
    )
    (:goal (= (basket) (set.mk (item_a item_b))))
)
