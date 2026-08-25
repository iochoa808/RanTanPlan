(define (problem sets-singleton-p0)
    (:domain sets-singleton)
    (:objects n0 n1 n2 - node)
    (:init
        (= (current) (set.mk (n0)))
    )
    (:goal (member n2 (current)))
)
