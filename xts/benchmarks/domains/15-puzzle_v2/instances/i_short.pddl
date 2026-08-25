(define (problem i_short)
    (:domain fifteen-puzzle)

    (:init
        (= (puzzle) (array.mk (( 1  5  2  3)
                               ( 4  6 10  7)
                               ( 8  9 11  0)
                               (12 13 14 15)))
        )
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
