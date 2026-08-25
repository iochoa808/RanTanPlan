(define (problem X-nested-forall-effect-01)
    (:domain X-nested-forall-effect)

    (:init (= (g) (array.mk (3 4) (5 2))))

    (:goal (and (= (read (g) (0) (0)) 0) (= (read (g) (1) (1)) 0)))
)
