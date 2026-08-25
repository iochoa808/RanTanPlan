;; visit_all_bool, 2x2 grid (4 places) — classic boolean-predicate goal.
;; Classical counterpart of visit_all_sets/instances/n2.pddl (same grid, same plans).
;; Robot starts at x0y0, which counts as already visited.
;; Adjacency is the static (connects ?x ?y) predicate — no set features.
;; Goal: 4 (visited ?wp) atoms — one per place, so the goal grows with the grid.

(define (problem visit_all_bool_2x2)
    (:domain visit-all-bool)
    (:objects
        x0y0 x0y1 x1y0 x1y1 - place
    )
    (:init
        (= (robot_at) x0y0)
        (visited x0y0)
        (connects x0y0 x0y1)
        (connects x0y0 x1y0)
        (connects x0y1 x0y0)
        (connects x0y1 x1y1)
        (connects x1y0 x1y1)
        (connects x1y0 x0y0)
        (connects x1y1 x1y0)
        (connects x1y1 x0y1)
    )
    (:goal
        (and
            (visited x0y0)
            (visited x0y1)
            (visited x1y0)
            (visited x1y1)
        )
    )
)
