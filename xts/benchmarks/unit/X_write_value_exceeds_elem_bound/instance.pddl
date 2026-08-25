(define (problem X-write-value-exceeds-elem-bound-01)
    (:domain X-write-value-exceeds-elem-bound)
    (:init
        (= (cells) (array.mk (0 1 2 3)))
    )
    (:goal (= (read (cells) 0) 99))
)
