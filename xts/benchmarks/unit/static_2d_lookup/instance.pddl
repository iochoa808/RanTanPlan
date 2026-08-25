;; adj (3×3, static):
;;   [[0, 1, 0],
;;    [0, 0, 1],
;;    [1, 0, 0]]
;;
;; After StaticFluentPass, adj disappears from the fluent set; every
;; (read (adj) from to) becomes a literal 0 or 1 in grounded actions.
;;
;; pos = 0 initially.  Goal: pos = 2.
;;
;; Only move(0,1) and move(1,2) have adj[from][to]=1.
;;
;; Expected plan (2 steps): move(0, 1), move(1, 2)

(define (problem static-adj-01)
    (:domain static-adj)

    (:init
        (= (adj) (array.mk ((0 1 0)
                              (0 0 1)
                              (1 0 0))))
        (= (pos) 0)
    )

    (:goal (= (pos) 2))
)
