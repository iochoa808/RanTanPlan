;; array.mk supplies 5 values but the declared array size is 3 — should error.
(define (problem X-array-mk-overcount-01)
    (:domain X-array-mk-overcount)
    (:init
        (= (cells) (array.mk (0 1 2 3 4)))
    )
    (:goal (= (read (cells) 0) 1))
)
