;; visit_all_sets, 2x2 grid (4 places) — set covering goal.
;; PDDL counterpart of xts/benchmarks/scaling/generators/visit_all_sets.py, generate(n=2).
;; Robot starts at x0y0; (visited) is initialised to {x0y0}.
;; Goal: one cardinality equality, constant size regardless of the grid.

(define (problem visit_all_sets_2x2)
    (:domain visit-all-sets)
    (:objects
        x0y0 x0y1 x1y0 x1y1 - place
    )
    (:init
        (= (robot_at) x0y0)
        (= (visited) (set.mk (x0y0)))
        (= (connects x0y0) (set.mk (x0y1 x1y0)))
        (= (connects x0y1) (set.mk (x0y0 x1y1)))
        (= (connects x1y0) (set.mk (x1y1 x0y0)))
        (= (connects x1y1) (set.mk (x1y0 x0y1)))
    )
    (:goal (= (cardinality (visited)) 4))
)
