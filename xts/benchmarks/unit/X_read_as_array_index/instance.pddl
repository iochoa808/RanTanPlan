(define (problem X-read-as-array-index-01)
    (:domain X-read-as-array-index)
    (:init
        (= (pointers) (array.mk (2 0 1)))
        (= (data)     (array.mk (7 3 5)))
        (= (result) 0)
    )
    (:goal (= (result) 5))
)
