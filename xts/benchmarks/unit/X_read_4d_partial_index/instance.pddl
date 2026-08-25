;; 4D tensor with partial index access (error expected).

(define (problem X-read-4d-partial-01)
    (:domain X-read-4d-partial)

    (:init
        (= (result) 0)
    )

    (:goal (= (result) 5))
)
