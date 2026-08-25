;; PDDL-XTS translation of pddl/test/no-mystery-sequential-optimal (transport-strips).
;; FEATURES: bounded integers + sets + object fluents.
;;   - fuellevel objects + (fuel ?t ?level) + (sum)/(fuelcost) predicates
;;     ->  bounded-int fuel gauge (fuel ?t) : (number 0 2); drive guards
;;         (>= (fuel ?t) 1) and decreases it (every edge costs one unit here).
;;   - (connected ?l1 ?l2) binary relation -> set fluent (connections ?l).
;;   - truck location (total function) -> object fluent (tloc ?t) - location.
;;   - package location stays boolean (package is at a place OR in a truck).
;;   Action-cost metric dropped (irrelevant to satisficing).

(define (domain no-mystery-xts)
    (:requirements :typing :sets :bounded-integers :object-fluents)
    (:types
        location truck package - object
        locset - (set location)
        flevel - (number 0 2)
    )
    (:predicates
        (pat ?p - package ?l - location)
        (pin ?p - package ?t - truck)
    )
    (:functions
        (connections ?l - location) - locset
        (fuel ?t - truck)           - flevel
        (tloc ?t - truck)           - location
    )

    (:action load
        :parameters (?p - package ?t - truck ?l - location)
        :precondition (and (= (tloc ?t) ?l) (pat ?p ?l))
        :effect (and (not (pat ?p ?l)) (pin ?p ?t))
    )
    (:action unload
        :parameters (?p - package ?t - truck ?l - location)
        :precondition (and (= (tloc ?t) ?l) (pin ?p ?t))
        :effect (and (pat ?p ?l) (not (pin ?p ?t)))
    )
    (:action drive
        :parameters (?t - truck ?l1 ?l2 - location)
        :precondition (and (member ?l2 (connections ?l1))
                           (= (tloc ?t) ?l1)
                           (>= (fuel ?t) 1))
        :effect (and (assign (tloc ?t) ?l2) (decrease (fuel ?t) 1))
    )
)
