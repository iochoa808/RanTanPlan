;; visit_all_sets, 3x3 grid (9 places) — set covering goal.
;; PDDL counterpart of xts/benchmarks/scaling/generators/visit_all_sets.py, generate(n=3).
;; Robot starts at x0y0; (visited) is initialised to {x0y0}.
;; Goal: one cardinality equality, constant size regardless of the grid.

(define (problem visit_all_sets_3x3)
    (:domain visit-all-sets)
    (:objects
        x0y0 x0y1 x0y2 x1y0 x1y1
        x1y2 x2y0 x2y1 x2y2 - place
    )
    (:init
        (= (robot_at) x0y0)
        (= (visited) (set.mk (x0y0)))
        (= (connects x0y0) (set.mk (x0y1 x1y0)))
        (= (connects x0y1) (set.mk (x0y2 x0y0 x1y1)))
        (= (connects x0y2) (set.mk (x0y1 x1y2)))
        (= (connects x1y0) (set.mk (x1y1 x2y0 x0y0)))
        (= (connects x1y1) (set.mk (x1y2 x1y0 x2y1 x0y1)))
        (= (connects x1y2) (set.mk (x1y1 x2y2 x0y2)))
        (= (connects x2y0) (set.mk (x2y1 x1y0)))
        (= (connects x2y1) (set.mk (x2y2 x2y0 x1y1)))
        (= (connects x2y2) (set.mk (x2y1 x1y2)))
    )
    (:goal (= (cardinality (visited)) 9))
)
