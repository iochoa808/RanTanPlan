;; PDDL-XTS translation of pddl/test/sokoban .../problem.pddl (mini-sokoban, 3x3).
;; pos-R-C -> cell (R-1, C-1).  player at pos-1-1 = (0,0); stone at pos-2-2 = (1,1);
;; goal IS-GOAL pos-3-3 -> stone at (2,2).

(define (problem mini-sokoban-xts)
    (:domain sokoban-xts)
    (:init
        (= (stones) (array.mk ((0 0 0)
                               (0 1 0)
                               (0 0 0))))
        (= (prow) 0)
        (= (pcol) 0)
    )
    (:goal (= (read (stones) (2) (2)) 1))
)
