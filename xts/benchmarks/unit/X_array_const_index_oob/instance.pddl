(define (problem X-array-const-index-oob-01)
    (:domain X-array-const-index-oob)
    (:init
        (= (cells)  (array.mk (1 2 3 4)))
        (= (result) 0)
    )
    ;; goal=3 is unreachable: read_oob returns 0 (CWA default), not 3
    (:goal (= (result) 3))
)
