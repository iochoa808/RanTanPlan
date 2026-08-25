;; PDDL-XTS translation of pddl/test/tetris .../problem.pddl (minimal-tetris).
;; square at p0-0 = (0,0); goal at_square square0 p1-1 -> square at (1,1).

(define (problem minimal-tetris-xts)
    (:domain tetris-xts)
    (:init
        (= (board) (array.mk ((1 0)
                              (0 0))))
        (= (srow) 0)
        (= (scol) 0)
    )
    (:goal (= (read (board) (1) (1)) 1))
)
