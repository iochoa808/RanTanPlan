;; PDDL-XTS translation of pddl/test/transport-sequential-optimal-strips.
;; FEATURES: bounded integers + sets + object fluents.
;;   - capacity-number objects + (capacity-predecessor) chain + (capacity ?v ?s)
;;     predicate  ->  bounded-int fluent (capacity ?v) : (number 0 4); pick-up
;;     decreases it (guard > 0), drop increases it.  This is the idiomatic XTS
;;     form of the STRIPS "successor-object" integer encoding.
;;   - (road ?l1 ?l2) binary relation -> per-location set fluent (roads ?l).
;;   - vehicle location (total function) -> object fluent (vat ?v) - location.
;;   - package location stays boolean (a package is either at a place OR in a
;;     vehicle — a partial relation).
;;   The action-cost metric (road-length / total-cost) is dropped: it does not
;;   affect satisficing solvability, which is the mode XTS array/set domains use.

(define (domain transport-xts)
    (:requirements :typing :sets :bounded-integers :object-fluents)
    (:types
        location vehicle package - object
        locset - (set location)
        cap    - (number 0 4)
    )
    (:predicates
        (pat ?p - package ?l - location)
        (pin ?p - package ?v - vehicle)
    )
    (:functions
        (roads ?l - location)   - locset
        (vat ?v - vehicle)      - location
        (capacity ?v - vehicle) - cap
    )

    (:action drive
        :parameters (?v - vehicle ?l1 ?l2 - location)
        :precondition (and (= (vat ?v) ?l1) (member ?l2 (roads ?l1)))
        :effect (assign (vat ?v) ?l2)
    )
    (:action pick-up
        :parameters (?v - vehicle ?l - location ?p - package)
        :precondition (and (= (vat ?v) ?l) (pat ?p ?l) (> (capacity ?v) 0))
        :effect (and (not (pat ?p ?l)) (pin ?p ?v) (decrease (capacity ?v) 1))
    )
    (:action drop
        :parameters (?v - vehicle ?l - location ?p - package)
        :precondition (and (= (vat ?v) ?l) (pin ?p ?v))
        :effect (and (not (pin ?p ?v)) (pat ?p ?l) (increase (capacity ?v) 1))
    )
)
