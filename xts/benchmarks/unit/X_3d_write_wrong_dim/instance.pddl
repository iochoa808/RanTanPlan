(define (problem test-3d-wrong-dim-01)
    (:domain test-3d-wrong-dim)

    (:init
        (= (tensor) (array.mk (((0 0)(0 0))
                                ((0 0)(0 0)))))
        (= (result) 0)
    )

    (:goal (= (result) 5))
)
