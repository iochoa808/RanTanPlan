;; hypercube starts all-zero (2×2×2×2).
;; Goal: hypercube equals the literal below — whole-array comparison.
;;
;; Non-zero cells: [0][0][0][0]=1, [1][1][0][1]=2.
;;
;; Plan: set(0,0,0,0,1), set(1,1,0,1,2)  — 2 steps.

(define (problem test-4d-whole-01)
    (:domain test-4d-whole)

    (:init
        (= (hypercube) (array.mk ((((0 0)(0 0))((0 0)(0 0)))
                                   (((0 0)(0 0))((0 0)(0 0))))))
    )

    (:goal
        (= (hypercube) (array.mk ((((1 0)(0 0))((0 0)(0 0)))
                                   (((0 0)(0 0))((0 2)(0 0))))))
    )
)
