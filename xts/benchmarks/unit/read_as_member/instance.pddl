;; Board: tiles = [7, 3, 9, 2].  Called numbers: {3, 9, 5}.
;; Positions 1 (tile=3) and 2 (tile=9) match => score 2.
;;
;; Plan: mark(1,3), mark(2,9)  — 2 steps.

(define (problem bingo-01)
    (:domain bingo)

    (:init
        (= (tiles)  (array.mk (7 3 9 2)))
        (= (called) (set.mk (3 9 5)))
        (= (score)  0)
    )

    (:goal (= (score) 2))
)
