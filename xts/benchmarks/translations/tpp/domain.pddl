;; PDDL-XTS translation of pddl/test/tpp (TPP-Metric).
;; FEATURES: bounded integers + object fluents.
;;   - quantities (on-sale, bought, request) -> bounded-int fluents (number 0 15).
;;   - truck location (loc ?t ?p) total function -> object fluent (loc-of ?t) - place.
;;   - purchasing/routing cost machinery (price, drive-cost, total-cost) dropped:
;;     it only feeds the minimize metric, which satisficing ignores.

(define (domain tpp-xts)
    (:requirements :typing :bounded-integers :object-fluents)
    (:types
        place locatable - object
        depot market - place
        truck goods - locatable
        qty - (number 0 15)
    )
    (:functions
        (loc-of ?t - truck)              - place
        (on-sale ?g - goods ?m - market) - qty
        (bought ?g - goods)              - qty
        (request ?g - goods)             - qty
    )

    (:action drive
        :parameters (?t - truck ?from ?to - place)
        :precondition (= (loc-of ?t) ?from)
        :effect (assign (loc-of ?t) ?to)
    )
    (:action buy-allneeded
        :parameters (?t - truck ?g - goods ?m - market)
        :precondition (and (= (loc-of ?t) ?m) (> (on-sale ?g ?m) 0)
                           (> (on-sale ?g ?m) (- (request ?g) (bought ?g))))
        :effect (and (decrease (on-sale ?g ?m) (- (request ?g) (bought ?g)))
                     (assign (bought ?g) (request ?g)))
    )
    (:action buy-all
        :parameters (?t - truck ?g - goods ?m - market)
        :precondition (and (= (loc-of ?t) ?m) (> (on-sale ?g ?m) 0)
                           (<= (on-sale ?g ?m) (- (request ?g) (bought ?g))))
        :effect (and (assign (on-sale ?g ?m) 0)
                     (increase (bought ?g) (on-sale ?g ?m)))
    )
)
