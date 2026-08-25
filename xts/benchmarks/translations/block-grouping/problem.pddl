;; PDDL-XTS translation of pddl/test/block-grouping/problem.pddl (3 blocks, 5x5).
(define (problem block-grouping-3-xts)
    (:domain block-grouping-xts)
    (:objects b1 b2 - block b3 - block)
    (:init
        (= (x b1) 1) (= (y b1) 3)
        (= (x b2) 2) (= (y b2) 4)
        (= (x b3) 4) (= (y b3) 3)
    )
    (:goal (and
        (= (x b1) (x b2)) (= (y b1) (y b2))
        (or (not (= (x b1) (x b3))) (not (= (y b1) (y b3))))
    ))
)
