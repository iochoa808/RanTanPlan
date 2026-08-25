;; dist_table (3×3, static):
;;   [[1, 3, 8],
;;    [2, 7, 4],
;;    [3, 5, 6]]
;;
;; After StaticFluentPass, dist_table disappears from the fluent set; every
;; (read (dist_table) i j) in grounded actions becomes a literal integer.
;;
;; cost = 0 initially.  Goal: cost = 7.
;;
;; Only record(1,1) has dist_table[1][1]=7 > 5, so it is the sole action
;; that can achieve cost=7.
;;
;; Expected plan (1 step): record(1, 1)

(define (problem static-lookup-01)
    (:domain static-lookup)

    (:init
        (= (dist_table) (array.mk ((1 3 8)
                                    (2 7 4)
                                    (3 5 6))))
        (= (cost) 0)
    )

    (:goal (= (cost) 7))
)
