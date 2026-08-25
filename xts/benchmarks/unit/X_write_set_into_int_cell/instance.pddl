;; Write a set into an integer array cell — error expected.

(define (problem X-set-into-int-01)
    (:domain X-set-into-int)

    (:init
        (= (cells) (array.mk (0 0 0)))
        (= (bag)   (set.mk (1 2 3)))
    )

    (:goal (= (read (cells) 0) 1))
)
