;; Test: PARAMETERIZED FLUENT + 2D ARRAY — (grid ?p - player) of type 2d-array.
;;
;; KEY PATTERN: a 2D array fluent parameterized by an object.
;;   All existing parameterized array fluents are 1D.
;;   This introduces (grid ?p - player) - grid-t  where grid-t = (array 2 2 val).
;;
;; Each player has their own independent 2×2 score grid.
;;   write_cell(?p, ?r, ?c, ?v): write value ?v into player ?p's cell [r][c].
;; Initial: both grids all-zero.
;; Goal: grid(player1)[0][1] = 5.
;; Plan: write_cell(player1, 0, 1, 5)  — 1 step.

(define (domain player-grid)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        player - object
        row    - (number 0 1)
        col    - (number 0 1)
        val    - (number 0 9)
        grid-t - (array 2 2 val)
    )

    (:functions
        (grid ?p - player) - grid-t
    )

    ;; Write value ?v into player ?p's cell [?r][?c].
    (:action write_cell
        :parameters (?p - player ?r - row ?c - col ?v - val)
        :precondition (= (read (grid ?p) ?r ?c) 0)
        :effect (write ((grid ?p) ?r ?c) ?v)
    )
)
