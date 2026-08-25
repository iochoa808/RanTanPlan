(define (problem X-set-mk-arithmetic-effect-01)
    (:domain X-set-mk-arithmetic-effect)
    (:init
        (= (chain) (set.mk (3)))
    )
    (:goal (member 4 (chain)))
)
