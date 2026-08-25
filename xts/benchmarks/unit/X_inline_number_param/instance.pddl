;; Inline (number lo hi) in :parameters — error expected.

(define (problem X-inline-number-param-01)
    (:domain X-inline-number-param)

    (:init
        (= (counter) 0)
    )

    (:goal (= (counter) 5))
)
