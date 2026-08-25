;; PDDL-XTS translation of pddl/test/settlers-strips/problem.pddl (simple-civ-v2).
;; (is-at cart1 ?) becomes (vehicle-at cart1) = nowhere (domain constant sentinel).
;; connected-by-land village port -> (= (land-conns village) (set.mk (port))).
;; All other connection sets start empty.
;; Vehicle resource fluents initialised to 0 (build-* assigns their initial values).

(define (problem simple-civ-v2-xts)
    (:domain civ-xts)
    (:objects
        village port - place
        cart1 - vehicle
    )
    (:init
        ;; Geography
        (woodland village)
        (mountain port)
        (by-coast port)

        ;; Connectivity: only land village<->port initially; no rail or sea.
        (= (land-conns village) (set.mk (port)))
        (= (land-conns port)    (set.mk (village)))
        (= (rail-conns village) (set.mk ()))
        (= (rail-conns port)    (set.mk ()))
        (= (sea-conns village)  (set.mk ()))
        (= (sea-conns port)     (set.mk ()))

        ;; Unbuilt vehicle: sentinel location, 0 space, 0 resources.
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

        ;; nowhere sentinel: a domain constant of type place; all place fluents
        ;; must be initialised for it (values are unused by any reachable action).
        (= (land-conns nowhere) (set.mk ()))
        (= (rail-conns nowhere) (set.mk ()))
        (= (sea-conns nowhere) (set.mk ()))
        (= (available timber nowhere) 0)
        (= (available wood   nowhere) 0)
        (= (available coal   nowhere) 0)
        (= (available stone  nowhere) 0)
        (= (available iron   nowhere) 0)
        (= (available ore    nowhere) 0)
        (= (housing nowhere) 0)

        ;; Global counters.
        (= (labour) 0)
        (= (resource-use) 0)
        (= (pollution) 0)
        (= (housing village) 0)
        (= (housing port)    0)
    )
    (:goal
        (and
            (has-cabin village)
            (has-quarry port)
            (> (available timber village) 0)
            (> (available stone port)    0)
            (is-cart cart1)
            (= (vehicle-at cart1) port)
        )
    )
)