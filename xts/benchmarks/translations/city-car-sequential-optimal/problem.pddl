;; PDDL-XTS translation of pddl/test/city-car-sequential-optimal/problem.pddl.
;; (same_line j1 j2) -> (= (line-neighbors j1) (set.mk (j2)))
;;     j2 has no same-line neighbors (no same_line j2 ?) in original, so
;;     (= (line-neighbors j2) (set.mk ()))
;; (diagonal ?) -> no diagonal relations in this instance; both empty sets.
;; (not (in_place road1)) omitted: CWA makes unmentioned predicates false.
;; (= (total-cost) 0) dropped: metric accumulator; satisficing mode.
;; Metric (minimize total-cost) dropped.
;; Plan: build_straight j1->j2, car_start j1, move_in, move_out, car_arrived j2 (5 steps).

(define (problem minimal-citycar-xts) (:domain citycar-xts)
(:objects
    car1    - car
    j1 j2   - junction
    garage1 - garage
    road1   - road)
(:init
    ;; Junction relationships as sets
    ;; same_line j1 j2: a straight road can be built from j1 to j2
    (= (line-neighbors j1) (set.mk (j2)))
    (= (line-neighbors j2) (set.mk ()))

    ;; No diagonal relationships in this instance
    (= (diag-neighbors j1) (set.mk ()))
    (= (diag-neighbors j2) (set.mk ()))

    ;; Junctions start clear
    (clear j1)
    (clear j2)

    ;; Garage at j1, car1 starts there
    (at_garage garage1 j1)
    (starting car1 garage1))

(:goal (arrived car1 j2))
)
