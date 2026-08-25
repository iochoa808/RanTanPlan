;; The domain errors at parse time — this instance is never reached.
(define (problem X-sem-forall-set-oob-01)
    (:domain X-sem-forall-set-oob)
    (:init
        (= (chain) (set.mk (8 9)))
    )
    (:goal ())
)
