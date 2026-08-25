(define (problem i3)
    (:domain fifteen-puzzle)

    (:init
        (= (puzzle) (array.mk ((14  7  8  2)
                               (13 11 10  4)
                               ( 9 12  5  0)
                               ( 3  6  1 15)))
        )
        (= (blank_row) 2)
        (= (blank_col) 3)
        (= (last_move) 0)
    )

    (:goal
        (= (puzzle) (array.mk (( 0  1  2  3)
                               ( 4  5  6  7)
                               ( 8  9 10 11)
                               (12 13 14 15)))
        )
    )
)
