;; PDDL-XTS translation of pddl/test/expedition.
;; FEATURES: object fluents + bounded integers.
;;   - (at ?s ?w) sled location (total function) -> object fluent (sled-at ?s) - waypoint.
;;   - sled_supplies / sled_capacity / waypoint_supplies -> bounded ints (number 0 9).
;;   - (is_next ?x ?y) stays a static relation (predicate).
;;   NOTE: the original sets waypoint_supplies to 1000 ("effectively infinite");
;;   here it is a small finite stock (5) sufficient for the plan, since bounding
;;   a fluent to 1000 would inflate the encoding with no behavioural difference.

(define (domain expedition-xts)
    (:requirements :typing :bounded-integers :object-fluents)
    (:types
        sled waypoint - object
        supply - (number 0 9)
    )
    (:predicates
        (is_next ?x ?y - waypoint)
    )
    (:functions
        (sled-at ?s - sled)            - waypoint
        (sled_supplies ?s - sled)      - supply
        (sled_capacity ?s - sled)      - supply
        (waypoint_supplies ?w - waypoint) - supply
    )

    (:action move_forwards
        :parameters (?s - sled ?w1 ?w2 - waypoint)
        :precondition (and (= (sled-at ?s) ?w1) (>= (sled_supplies ?s) 1) (is_next ?w1 ?w2))
        :effect (and (assign (sled-at ?s) ?w2) (decrease (sled_supplies ?s) 1))
    )
    (:action move_backwards
        :parameters (?s - sled ?w1 ?w2 - waypoint)
        :precondition (and (= (sled-at ?s) ?w1) (>= (sled_supplies ?s) 1) (is_next ?w2 ?w1))
        :effect (and (assign (sled-at ?s) ?w2) (decrease (sled_supplies ?s) 1))
    )
    (:action store_supplies
        :parameters (?s - sled ?w - waypoint)
        :precondition (and (= (sled-at ?s) ?w) (>= (sled_supplies ?s) 1))
        :effect (and (increase (waypoint_supplies ?w) 1) (decrease (sled_supplies ?s) 1))
    )
    (:action retrieve_supplies
        :parameters (?s - sled ?w - waypoint)
        :precondition (and (= (sled-at ?s) ?w) (>= (waypoint_supplies ?w) 1)
                           (> (sled_capacity ?s) (sled_supplies ?s)))
        :effect (and (decrease (waypoint_supplies ?w) 1) (increase (sled_supplies ?s) 1))
    )
)
