;; 2D array size mismatch in effect — error expected.

(define (problem X-size-mismatch-2d-01)
    (:domain X-size-mismatch-2d)

    (:init
        (= (grid) (array.mk (0 0 0) (0 0 0)))
    )

    (:goal (= (read (grid) 0 0) 1))
)
