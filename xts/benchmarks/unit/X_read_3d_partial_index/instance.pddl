(define (problem x-read-3d-partial-01)
    (:domain x-read-3d-partial)

    (:init
        (= (tensor) (array.mk (((1 2)(3 4))((5 6)(7 8)))))
        (= (result) 0)
    )

    (:goal (= (result) 5))
)
