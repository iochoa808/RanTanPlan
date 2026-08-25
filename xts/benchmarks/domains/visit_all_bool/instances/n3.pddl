;; visit_all_bool, 3x3 grid (9 places) — classic boolean-predicate goal.
;; Classical counterpart of visit_all_sets/instances/n3.pddl (same grid, same plans).
;; Robot starts at x0y0, which counts as already visited.
;; Adjacency is the static (connects ?x ?y) predicate — no set features.
;; Goal: 9 (visited ?wp) atoms — one per place, so the goal grows with the grid.

(define (problem visit_all_bool_3x3)
    (:domain visit-all-bool)
    (:objects
        x0y0 x0y1 x0y2 x1y0 x1y1
        x1y2 x2y0 x2y1 x2y2 - place
    )
    (:init
        (= (robot_at) x0y0)
        (visited x0y0)
        (connects x0y0 x0y1)
        (connects x0y0 x1y0)
        (connects x0y1 x0y2)
        (connects x0y1 x0y0)
        (connects x0y1 x1y1)
        (connects x0y2 x0y1)
        (connects x0y2 x1y2)
        (connects x1y0 x1y1)
        (connects x1y0 x2y0)
        (connects x1y0 x0y0)
        (connects x1y1 x1y2)
        (connects x1y1 x1y0)
        (connects x1y1 x2y1)
        (connects x1y1 x0y1)
        (connects x1y2 x1y1)
        (connects x1y2 x2y2)
        (connects x1y2 x0y2)
        (connects x2y0 x2y1)
        (connects x2y0 x1y0)
        (connects x2y1 x2y2)
        (connects x2y1 x2y0)
        (connects x2y1 x1y1)
        (connects x2y2 x2y1)
        (connects x2y2 x1y2)
    )
    (:goal
        (and
            (visited x0y0)
            (visited x0y1)
            (visited x0y2)
            (visited x1y0)
            (visited x1y1)
            (visited x1y2)
            (visited x2y0)
            (visited x2y1)
            (visited x2y2)
        )
    )
)
