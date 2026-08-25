(define (problem X-fluent-as-array-index-01)
    (:domain X-fluent-as-array-index)
    (:init
        (= (cells) (array.mk (5 3 7 1)))
        (= (head) 2)
        (= (result) 0)
    )
    (:goal (= (result) 7))
)
