;; 2×2×2 cube; cube[d][r][c] = p{4d+2r+c}.
;; Goal: swap (0,0) pair and (1,1) pair.
;; Plan: swap-col(0,0,p0,p1), swap-col(1,1,p6,p7)  — 2 steps.

(define (problem cube-01)
    (:domain cube-swap)

    (:objects p0 p1 p2 p3 p4 p5 p6 p7 - person)

    (:init
        (= (cube) (array.mk (((p0 p1)(p2 p3))
                              ((p4 p5)(p6 p7)))))
    )

    (:goal (and
        (= (read (cube) 0 0 0) p1)
        (= (read (cube) 0 0 1) p0)
        (= (read (cube) 1 1 0) p7)
        (= (read (cube) 1 1 1) p6)
    ))
)
