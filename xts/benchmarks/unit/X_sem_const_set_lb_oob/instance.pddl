;; The domain errors at parse time — this instance is never reached.
(define (problem X-sem-const-set-lb-oob-01)
    (:domain X-sem-const-set-lb-oob)
    (:init
        (= (restricted) (set.mk ()))
    )
    (:goal ())
)
