(define (problem X-sem-member-wrong-elem-type-01)
    (:domain X-sem-member-wrong-elem-type)
    (:init
        (= (int_bag) (set.mk (1 5)))
        (= (obj_bag) (set.mk (item_a item_b)))
    )
    (:goal (found))
)
