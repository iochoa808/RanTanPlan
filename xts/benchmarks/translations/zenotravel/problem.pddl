;; PDDL-XTS translation of pddl/test/zenotravel/problem.pddl (ZTRAVEL-1-2).
;; (located plane1 cityX) -> (= (aircraft-at plane1) cityX)  [object fluent]
;; (located person1 cityX) -> (at person1 cityX)             [boolean, partial]
;; (= (onboard plane1) 0) -> bounded int obd_t [0,8]
;; (= (total-fuel-used) 0) -> dropped (metric accumulator; satisficing mode)
;; All distance/burn/capacity/zoom-limit values kept as plain numeric (too large to bound).
;; Metric (minimize total-fuel-used) dropped.

(define (problem ZTRAVEL-1-2-xts) (:domain zenotravel-xts)
(:objects
    plane1   - aircraft
    person1 person2 person3 - person
    city0 city1 city2 - city)
(:init
    ;; Aircraft position (object fluent — always at exactly one city)
    (= (aircraft-at plane1) city0)

    ;; Aircraft parameters (plain numeric — too large/static to bound)
    (= (capacity  plane1) 6000)
    (= (fuel      plane1) 4000)
    (= (slow-burn plane1) 4)
    (= (fast-burn plane1) 15)
    (= (zoom-limit plane1) 8)

    ;; Passenger count (bounded int obd_t)
    (= (onboard plane1) 0)

    ;; Person locations (boolean — partial: persons can be in-plane too)
    (at person1 city0)
    (at person2 city0)
    (at person3 city1)

    ;; Distances (static; StaticFluentPass folds these into action costs)
    (= (distance city0 city0) 0)
    (= (distance city0 city1) 678)
    (= (distance city0 city2) 775)
    (= (distance city1 city0) 678)
    (= (distance city1 city1) 0)
    (= (distance city1 city2) 810)
    (= (distance city2 city0) 775)
    (= (distance city2 city1) 810)
    (= (distance city2 city2) 0))

(:goal (and
    (at person1 city2)
    (at person2 city1)
    (at person3 city2)))
)
