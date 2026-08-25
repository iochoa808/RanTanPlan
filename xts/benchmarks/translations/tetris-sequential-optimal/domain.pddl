;; PDDL-XTS translation of pddl/test/tetris-sequential-optimal (minimal instance).
;; FEATURE: 2D arrays + bounded integers.
;;   The minimal instance uses only a single one_square piece on a 2x2 grid
;;   (positions pR-C), so we index the board directly:
;;     (board) : (array 2 2 (number 0 1))  ; 1 = the square occupies cell (r,c)
;;     (srow)(scol) : (number 0 1)         ; the square's coordinates
;;   The 4-neighbour (connected) relation becomes +/-1 on a coordinate.
;;   NOTE: the two_straight and right_l pieces (and rotation) of the full Tetris
;;   domain are omitted because this instance contains none; they would need
;;   multi-cell occupancy patterns over the same array.

(define (domain tetris-xts)
    (:requirements :arrays :bounded-integers)
    (:types
        size    - (number 0 1)
        cell    - (number 0 1)
        board_t - (array 2 2 cell)
    )
    (:functions
        (board) - board_t
        (srow)  - size
        (scol)  - size
    )

    (:action move-down
        :parameters (?sr ?sc - size)
        :precondition (and (= (srow) ?sr) (= (scol) ?sc)
                           (= (read (board) (+ ?sr 1) ?sc) 0))
        :effect (and (write ((board) ?sr ?sc) 0)
                     (write ((board) (+ ?sr 1) ?sc) 1)
                     (assign (srow) (+ ?sr 1)))
    )
    (:action move-up
        :parameters (?sr ?sc - size)
        :precondition (and (= (srow) ?sr) (= (scol) ?sc)
                           (= (read (board) (- ?sr 1) ?sc) 0))
        :effect (and (write ((board) ?sr ?sc) 0)
                     (write ((board) (- ?sr 1) ?sc) 1)
                     (assign (srow) (- ?sr 1)))
    )
    (:action move-right
        :parameters (?sr ?sc - size)
        :precondition (and (= (srow) ?sr) (= (scol) ?sc)
                           (= (read (board) ?sr (+ ?sc 1)) 0))
        :effect (and (write ((board) ?sr ?sc) 0)
                     (write ((board) ?sr (+ ?sc 1)) 1)
                     (assign (scol) (+ ?sc 1)))
    )
    (:action move-left
        :parameters (?sr ?sc - size)
        :precondition (and (= (srow) ?sr) (= (scol) ?sc)
                           (= (read (board) ?sr (- ?sc 1)) 0))
        :effect (and (write ((board) ?sr ?sc) 0)
                     (write ((board) ?sr (- ?sc 1)) 1)
                     (assign (scol) (- ?sc 1)))
    )
)
