;; src = [3, 7], dst = [0, 0].  Goal: dst[1] = 7.
;; Satisficing: SOLVED in 1 step (copy).  Optimal: false UNSOLVABLE_PROVEN.

(define (problem bnb-optimal-unsound-01)
    (:domain bnb-optimal-unsound)

    (:init
        (= (src) (array.mk (3 7)))
        (= (dst) (array.mk (0 0)))
    )

    (:goal (= (read (dst) (1)) 7))
)
