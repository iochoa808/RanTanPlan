;; PDDL-XTS translation of pddl/test/visit-all-sequential-optimal/problem.pddl (grid-2).
;; Adjacency lists become per-place set.mk literals; visited starts as {loc-x1-y1}.

(define (problem grid-2-xts)
    (:domain grid-visit-all-xts)
    (:objects
        loc-x0-y0 loc-x0-y1 loc-x1-y0 loc-x1-y1 - place
    )
    (:init
        (= (robot-at) loc-x1-y1)
        (= (visited) (set.mk (loc-x1-y1)))
        (= (connects loc-x0-y0) (set.mk (loc-x1-y0 loc-x0-y1)))
        (= (connects loc-x0-y1) (set.mk (loc-x1-y1 loc-x0-y0)))
        (= (connects loc-x1-y0) (set.mk (loc-x0-y0 loc-x1-y1)))
        (= (connects loc-x1-y1) (set.mk (loc-x0-y1 loc-x1-y0)))
    )
    (:goal (and (member loc-x0-y0 (visited))
                (member loc-x0-y1 (visited))
                (member loc-x1-y0 (visited))
                (member loc-x1-y1 (visited))))
)
