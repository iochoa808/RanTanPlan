;; Two players; both grids start all-zero.
;; Goal: grid(player1)[0][1] = 5.
;; Plan: write_cell(player1, 0, 1, 5)  — 1 step.

(define (problem player-grid-01)
    (:domain player-grid)

    (:objects
        player1 player2 - player
    )

    (:init
        (= (grid player1) (array.mk (0 0) (0 0)))
        (= (grid player2) (array.mk (0 0) (0 0)))
    )

    (:goal (= (read (grid player1) 0 1) 5))
)
