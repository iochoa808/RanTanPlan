;; PDDL-XTS translation of pddl/test/settlers/problem.pddl (simple-civ).
;; (available ?r ?p) -> bounded int (all resources initialized to given values).
;; connected-by-land village port -> (= (land-conns village) (set.mk (port))).
;; All other connection sets start empty (no rail or sea connections initially).
;; cart1 is a potential vehicle (intended for train/ship, never built in goal).
;; (vehicle-at cart1) = nowhere (domain constant sentinel for all potential vehicles).
;; carts-at is 0 everywhere (no anonymous carts in this instance).
;; Metric (minimize labour) dropped; satisficing mode.
;; Minimal plan: build-cabin village / build-quarry port / break-stone port (3 steps).
;;   timber at village already > 0 from init, so fell-timber is not needed.

(define (problem simple-civ-xts) (:domain civ-xts)
(:objects
    village port - place
    cart1 - vehicle)
(:init
    ;; Geography
    (woodland village)
    (mountain port)
    (by-coast port)

    ;; Connectivity: only land village<->port; no rail or sea.
    (= (land-conns village) (set.mk (port)))
    (= (land-conns port)    (set.mk (village)))
    (= (rail-conns village) (set.mk ()))
    (= (rail-conns port)    (set.mk ()))
    (= (sea-conns village)  (set.mk ()))
    (= (sea-conns port)     (set.mk ()))

    ;; Potential vehicle: sentinel location, no space, no resources.
    (potential cart1)
    (= (vehicle-at cart1)        nowhere)
    (= (space-in cart1)          0)
    (= (available timber cart1)  0)
    (= (available wood   cart1)  0)
    (= (available coal   cart1)  0)
    (= (available stone  cart1)  0)
    (= (available iron   cart1)  0)
    (= (available ore    cart1)  0)

    ;; Initial resources at places.
    (= (available timber village) 3)
    (= (available wood   village) 0)
    (= (available coal   village) 0)
    (= (available stone  village) 0)
    (= (available iron   village) 0)
    (= (available ore    village) 0)

    (= (available timber port) 0)
    (= (available wood   port) 0)
    (= (available coal   port) 0)
    (= (available stone  port) 0)
    (= (available iron   port) 0)
    (= (available ore    port) 0)

    ;; Anonymous cart counts (none initially).
    (= (carts-at village) 0)
    (= (carts-at port)    0)

    ;; Global counters.
    (= (labour) 0)
    (= (resource-use) 0)
    (= (pollution) 0)
    (= (housing village) 0)
    (= (housing port)    0)

    ;; nowhere sentinel: domain constant of type place; all place-typed fluents
    ;; must be initialised for it (values are unreachable by any reachable action).
    (= (land-conns nowhere) (set.mk ()))
    (= (rail-conns nowhere) (set.mk ()))
    (= (sea-conns nowhere)  (set.mk ()))
    (= (available timber nowhere) 0)
    (= (available wood   nowhere) 0)
    (= (available coal   nowhere) 0)
    (= (available stone  nowhere) 0)
    (= (available iron   nowhere) 0)
    (= (available ore    nowhere) 0)
    (= (carts-at nowhere) 0)
    (= (housing nowhere)  0)
)
(:goal
    (and
        (has-cabin village)
        (has-quarry port)
        (> (available timber village) 0)
        (> (available stone port)    0)))
)
