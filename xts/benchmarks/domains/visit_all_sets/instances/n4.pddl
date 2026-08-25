;; visit_all_sets, 4x4 grid (16 places) — set covering goal.
;; PDDL counterpart of xts/benchmarks/scaling/generators/visit_all_sets.py, generate(n=4).
;; Robot starts at x0y0; (visited) is initialised to {x0y0}.
;; Goal: one cardinality equality, constant size regardless of the grid.

(define (problem visit_all_sets_4x4)
    (:domain visit-all-sets)
    (:objects
        x0y0 x0y1 x0y2 x0y3 x1y0
        x1y1 x1y2 x1y3 x2y0 x2y1
        x2y2 x2y3 x3y0 x3y1 x3y2
        x3y3 - place
    )
    (:init
        (= (robot_at) x0y0)
        (= (visited) (set.mk (x0y0)))
        (= (connects x0y0) (set.mk (x0y1 x1y0)))
        (= (connects x0y1) (set.mk (x0y2 x0y0 x1y1)))
        (= (connects x0y2) (set.mk (x0y3 x0y1 x1y2)))
        (= (connects x0y3) (set.mk (x0y2 x1y3)))
        (= (connects x1y0) (set.mk (x1y1 x2y0 x0y0)))
        (= (connects x1y1) (set.mk (x1y2 x1y0 x2y1 x0y1)))
        (= (connects x1y2) (set.mk (x1y3 x1y1 x2y2 x0y2)))
        (= (connects x1y3) (set.mk (x1y2 x2y3 x0y3)))
        (= (connects x2y0) (set.mk (x2y1 x3y0 x1y0)))
        (= (connects x2y1) (set.mk (x2y2 x2y0 x3y1 x1y1)))
        (= (connects x2y2) (set.mk (x2y3 x2y1 x3y2 x1y2)))
        (= (connects x2y3) (set.mk (x2y2 x3y3 x1y3)))
        (= (connects x3y0) (set.mk (x3y1 x2y0)))
        (= (connects x3y1) (set.mk (x3y2 x3y0 x2y1)))
        (= (connects x3y2) (set.mk (x3y3 x3y1 x2y2)))
        (= (connects x3y3) (set.mk (x3y2 x2y3)))
    )
    (:goal (= (cardinality (visited)) 16))
)
