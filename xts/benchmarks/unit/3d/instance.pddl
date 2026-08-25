;; tensor = [[[0,0],[0,0]],[[0,0],[0,0]]] — all zeros.
;; Goal: tensor[0][1][0] = 3  AND  tensor[1][0][1] = 2.
;;
;; Plan: set(0,1,0,3), set(1,0,1,2)  — 2 steps.

(define (problem test-3d-01)
    (:domain test-3d)

    (:init
        (= (tensor) (array.mk (((0 0)(0 0))
                                ((0 0)(0 0)))))
    )

    (:goal
        (and
            (= (read (tensor) (0) (1) (0)) 3)
            (= (read (tensor) (1) (0) (1)) 2)
        )
    )
)
