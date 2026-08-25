;; a=4, b=5 → combine produces c=9, out of [0,5].
;; a=3, b=4 → multiply produces c=12, out of [0,5].
(define (problem X-bounded-arith-overflow-01)
    (:domain X-bounded-arith-overflow)
    (:init
        (= (a) 4)
        (= (b) 5)
        (= (c) 0)
    )
    (:goal (= (c) 9))
)
