;; src=[[1,2,3],[4,5,6]], dst=all-zeros.  Goal: dst[1][2]=6.
;; Plan: copy_all()  — 1 step.

(define (problem sv-assign-2d-01)
    (:domain sv-assign-2d)

    (:init
        (= (src) (array.mk (1 2 3) (4 5 6)))
        (= (dst) (array.mk (0 0 0) (0 0 0)))
    )

    (:goal (= (read (dst) 1 2) 6))
)
