;; PDDL-XTS translation of pddl/test/sdac-zero-bound.
;; FEATURE: bounded integers. Identical to sdac-simple-xts; only the initial
;; value differs (0 instead of 1).

(define (domain sdac-zero-bound-xts)
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
