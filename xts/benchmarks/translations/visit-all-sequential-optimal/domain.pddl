;; PDDL-XTS translation of pddl/test/visit-all-sequential-optimal (grid-visit-all).
;; FEATURES: sets + object fluents.
;;   - (at-robot ?x)   total function  -> object fluent (robot-at) - place.
;;   - (connected ?x ?y) binary relation -> per-place set fluent
;;       (connects ?x) - placeset ; ?y reachable iff (member ?y (connects ?x)).
;;   - (visited ?x)    growing collection -> single set fluent (visited) - placeset;
;;       move adds the new place; the goal tests membership.

(define (domain grid-visit-all-xts)
    (:requirements :typing :sets :object-fluents)
    (:types
        place    - object
        placeset - (set place)
    )

    (:functions
        (robot-at)            - place
        (connects ?x - place) - placeset    ;; static adjacency
        (visited)             - placeset
    )

    (:action move
        :parameters (?curpos ?nextpos - place)
        :precondition (and (= (robot-at) ?curpos)
                           (member ?nextpos (connects ?curpos)))
        :effect (and (assign (robot-at) ?nextpos)
                     (add ?nextpos (visited)))
    )
)
