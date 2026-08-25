;; cells = [{0,1,2,3}, {0,2}, {1,3,4}].  big = 0.
;;
;; cells[0] has 4 elements (>= 3) — big.
;; cells[1] has 2 elements       — not big.
;; cells[2] has 3 elements (>= 3) — big.
;; Goal: big = 2.
;;
;; Plan: count_big(0), count_big(2)  — 2 steps.

(define (problem cell-inspector-01)
    (:domain cell-inspector)

    (:init
        (= (cells) (array.mk ((set.mk (0 1 2 3))
                              (set.mk (0 2))
                              (set.mk (1 3 4)))))
        (= (big) 0)
    )

    (:goal (= (big) 2))
)
