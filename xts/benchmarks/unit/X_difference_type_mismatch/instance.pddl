(define (problem x-difference-type-mismatch-01)
    (:domain x-difference-type-mismatch)

    (:objects item-a item-b - item)

    (:init
        (= (obj-bag) (set.mk (item-a item-b)))
        (= (int-bag) (set.mk (1 3)))
        (= (result)  (set.mk ()))
    )

    (:goal (done))
)
