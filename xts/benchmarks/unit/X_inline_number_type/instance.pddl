;; Inline (number lo hi) in :functions — error expected.

(define (problem X-inline-number-type-01)
    (:domain X-inline-number-type)

    (:init
        (= (f) 0)
    )

    (:goal (= (f) 3))
)
