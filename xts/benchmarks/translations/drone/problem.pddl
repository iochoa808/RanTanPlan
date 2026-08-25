;; PDDL-XTS translation of pddl/test/drone/problem.pddl (1x1x2 points).
(define (problem drone-1x1x2-xts)
    (:domain drone-xts)
    (:objects
        x0y0z0 x0y0z1 - location
    )
    (:init
        (= (x) 0) (= (y) 0) (= (z) 0)
        (= (xl x0y0z0) 0) (= (yl x0y0z0) 0) (= (zl x0y0z0) 0)
        (= (xl x0y0z1) 0) (= (yl x0y0z1) 0) (= (zl x0y0z1) 1)
        (= (battery-level) 9) (= (battery-level-full) 9)
    )
    (:goal (and (visited x0y0z0) (visited x0y0z1)
                (= (x) 0) (= (y) 0) (= (z) 0)))
)
