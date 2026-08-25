;; PDDL-XTS translation of pddl/test/tpp-propositional.
;; FEATURES: bounded integers + sets + object fluents.
;;   The propositional TPP encodes quantities as `level` objects threaded through
;;   a (next ?l1 ?l2) successor chain: loaded/ready-to-load/stored/on-sale are all
;;   indexed by level.  In XTS each becomes a bounded-int counter (number 0 1)
;;   (this instance has two levels), and load/buy/unload become +/-1 transfers.
;;   - (at ?t ?p) total function -> object fluent (truck-at ?t) - place.
;;   - (connected ?p1 ?p2) relation -> set fluent (connections ?p).

(define (domain tpp-prop-xts)
    (:requirements :typing :sets :bounded-integers :object-fluents)
    (:types
        place locatable - object
        depot market - place
        truck goods - locatable
        lvl - (number 0 1)
        placeset - (set place)
    )
    (:functions
        (truck-at ?t - truck)             - place
        (connections ?p - place)          - placeset
        (loaded ?g - goods ?t - truck)    - lvl
        (ready-to-load ?g - goods ?m - market) - lvl
        (stored ?g - goods)               - lvl
        (on-sale ?g - goods ?m - market)  - lvl
    )

    (:action drive
        :parameters (?t - truck ?from ?to - place)
        :precondition (and (= (truck-at ?t) ?from) (member ?to (connections ?from)))
        :effect (assign (truck-at ?t) ?to)
    )
    (:action load
        :parameters (?g - goods ?t - truck ?m - market)
        :precondition (and (= (truck-at ?t) ?m) (>= (ready-to-load ?g ?m) 1) (< (loaded ?g ?t) 1))
        :effect (and (decrease (ready-to-load ?g ?m) 1) (increase (loaded ?g ?t) 1))
    )
    (:action unload
        :parameters (?g - goods ?t - truck ?d - depot)
        :precondition (and (= (truck-at ?t) ?d) (>= (loaded ?g ?t) 1) (< (stored ?g) 1))
        :effect (and (decrease (loaded ?g ?t) 1) (increase (stored ?g) 1))
    )
    (:action buy
        :parameters (?t - truck ?g - goods ?m - market)
        :precondition (and (= (truck-at ?t) ?m) (>= (on-sale ?g ?m) 1) (< (ready-to-load ?g ?m) 1))
        :effect (and (decrease (on-sale ?g ?m) 1) (increase (ready-to-load ?g ?m) 1))
    )
)
