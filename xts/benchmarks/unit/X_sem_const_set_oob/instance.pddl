;; The domain errors at parse time — this instance is never reached.
(define (problem X-sem-const-set-oob-01)
    (:domain X-sem-const-set-oob)
    (:init
        (= (chain) (set.mk ()))
    )
    (:goal ())
)
