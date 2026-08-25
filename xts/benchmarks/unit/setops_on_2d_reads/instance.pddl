;; cells row 0 = [{0,1}, {2},    {3,4}  ].   (big: 0, 2 — 2 big cells)
;; cells row 1 = [{1,2}, {0,2,4},{3}   ].   (big: 0, 1 — 2 big cells)
;; big = 0.
;;
;; Cells with >= 2 elements: (0,0), (0,2), (1,0), (1,1) — 4 cells.
;; Goal: big = 4.
;;
;; Plan: count_big for each of the 4 qualifying cells  — 4 steps.

(define (problem grid-inspector-01)
    (:domain grid-inspector)

    (:init
        (= (cells) (array.mk (((set.mk (0 1)) (set.mk (2))     (set.mk (3 4)))
                               ((set.mk (1 2)) (set.mk (0 2 4)) (set.mk (3))))))
        (= (big) 0)
    )

    (:goal (= (big) 4))
)
