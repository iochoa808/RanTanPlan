;; cells = [7, 2, 8]; two cells (0 and 2) exceed 5.
;; Goal: hits = 2.
;;
;; Expected plan (2 steps): two scans that each hit a >5 cell.

(define (problem cond-arrread-01)
    (:domain cond-arrread)

    (:init
        (= (cells) (array.mk (7 2 8)))
        (= (hits) 0)
    )

    (:goal (= (hits) 2))
)
