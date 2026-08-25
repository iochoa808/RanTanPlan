;; The domain errors at parse time — this instance is never reached.
(define (problem X-sem-set-remove-oob-01)
    (:domain X-sem-set-remove-oob)
    (:init
        (= (chain) (set.mk (3 7)))
    )
    (:goal ())
)
