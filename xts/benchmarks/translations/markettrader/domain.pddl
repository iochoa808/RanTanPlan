;; PDDL-XTS translation of pddl/test/markettrader (Trader).
;; FEATURE: bounded integers + object fluents + sets.
;;   cash / capacity / on-sale / bought / prices / drive-cost -> bounded ints.
;;   - (at ?t ?p) camel location -> object fluent (camel-at ?t) - market.
;;   - (can-drive ?from ?to) relation -> set fluent (roads ?m) - marketset.
;;   sellprice is dropped (the sell action uses price, as in the original).
;;   (All places in this instance are markets, so the place/market split collapses.)

(define (domain trader-xts)
    (:requirements :typing :bounded-integers :object-fluents)
    (:types
        place locatable - object
        market - place
        camel goods - locatable
        money - (number 0 63)
        cap   - (number 0 25)
        stock - (number 0 15)
        marketset - (set market)
    )
    (:functions
        (camel-at ?t - camel)            - market
        (roads ?m - market)              - marketset
        (on-sale ?g - goods ?m - market) - stock
        (drive-cost ?p1 ?p2 - market)    - money
        (price ?g - goods ?m - market)   - money
        (bought ?g - goods)              - stock
        (cash)                           - money
        (capacity)                       - cap
    )

    (:action travel
        :parameters (?t - camel ?from ?to - market)
        :precondition (and (member ?to (roads ?from))
                           (>= (cash) (drive-cost ?from ?to))
                           (= (camel-at ?t) ?from))
        :effect (and (decrease (cash) (drive-cost ?from ?to)) (assign (camel-at ?t) ?to))
    )
    (:action buy
        :parameters (?t - camel ?g - goods ?m - market)
        :precondition (and (= (camel-at ?t) ?m) (<= (+ 7 (price ?g ?m)) (cash))
                           (>= (capacity) 1) (> (on-sale ?g ?m) 0))
        :effect (and (decrease (capacity) 1) (increase (bought ?g) 1)
                     (decrease (cash) (price ?g ?m)) (decrease (on-sale ?g ?m) 1))
    )
    (:action sell
        :parameters (?t - camel ?g - goods ?m - market)
        :precondition (and (= (camel-at ?t) ?m) (>= (bought ?g) 1) (< (cash) 63))
        :effect (and (increase (capacity) 1) (decrease (bought ?g) 1)
                     (increase (cash) (price ?g ?m)))
    )
)
