;; a = [0].  Goal: a[0] = 5.
;; Expected plan (1 step): bump()

(define (problem size1-01)
    (:domain size1)

    (:init
        (= (a) (array.mk (0)))
    )

    (:goal
        (= (read (a) (0)) 5)
    )
)
