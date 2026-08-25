;; hypercube = all zeros (2×2×2×2).
;; Goal: hypercube[0][1][0][1] = 2  AND  hypercube[1][0][1][0] = 1.
;;
;; Plan: set(0,1,0,1,2), set(1,0,1,0,1)  — 2 steps.

(define (problem test-4d-01)
    (:domain test-4d)

    (:init
        (= (hypercube) (array.mk ((((0 0)(0 0))((0 0)(0 0)))
                                   (((0 0)(0 0))((0 0)(0 0))))))
    )

    (:goal
        (and
            (= (read (hypercube) (0) (1) (0) (1)) 2)
            (= (read (hypercube) (1) (0) (1) (0)) 1)
        )
    )
)
