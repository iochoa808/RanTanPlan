;; tensor starts all-zero.
;; Goal: tensor equals [[[1,2],[3,0]],[[0,4],[0,0]]] — whole-array literal comparison.
;;
;; Plan: set(0,0,0,1), set(0,0,1,2), set(0,1,0,3), set(1,1,1,4) — 4 steps.

(define (problem test-3d-whole-01)
    (:domain test-3d-whole)

    (:init
        (= (tensor) (array.mk (((0 0)(0 0))
                                ((0 0)(0 0)))))
    )

    (:goal
        (= (tensor) (array.mk (((1 2)(3 0))
                                ((0 4)(0 0)))))
    )
)
