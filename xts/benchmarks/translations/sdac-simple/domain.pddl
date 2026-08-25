;; PDDL-XTS translation of pddl/test/sdac-simple.
;; FEATURE: bounded integers.
;;   (value ?c) -> (number 0 11) (the increment guard caps it at 11).
;;   The state-dependent total-cost is dropped (satisficing ignores the metric).

(define (domain sdac-simple-xts)
    (:requirements :numeric-fluents :typing :bounded-integers)
    (:types
        counter - object
        cval - (number 0 11)
    )
    (:predicates (done) (is-target ?c - counter))
    (:functions (value ?c - counter) - cval)

    (:action increment
        :parameters (?c - counter)
        :precondition (<= (value ?c) 10)
        :effect (increase (value ?c) 1)
    )
    (:action finish
        :parameters (?c - counter)
        :precondition (and (is-target ?c) (>= (value ?c) 3))
        :effect (done)
    )
)
