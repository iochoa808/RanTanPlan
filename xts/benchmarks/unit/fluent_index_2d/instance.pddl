;; board = [[1, 3, 0], [2, 0, 4]].
;; Goal: board[1][1] = 3.
;;
;; Plan: inc(1,1), inc(1,1), inc(1,1)  — 3 steps.

(define (problem score-grid-01)
    (:domain score-grid)

    (:init
        (= (board) (array.mk ((1 3 0)
                               (2 0 4))))
    )

    (:goal (= (read (board) 1 1) 3))
)
