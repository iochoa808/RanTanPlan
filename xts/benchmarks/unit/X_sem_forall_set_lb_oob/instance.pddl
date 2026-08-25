;; The domain errors at parse time — this instance is never reached.
(define (problem X-sem-forall-set-lb-oob-01)
    (:domain X-sem-forall-set-lb-oob)
    (:init
        (= (zone) (set.mk (2 3)))
    )
    (:goal ())
)
