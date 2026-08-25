(define (domain delivery-xts)
    (:requirements :typing :sets :bounded-integers :object-fluents)

    (:types
        
        place       - (array
        locatable   - object
        depot       - place
        distributor - place
        truck       - locatable
        hoist       - locatable
        surface     - locatable
        pallet      - surface
        crate       - surface

    )
    (:predicates
        (at ?i - item ?x - room)
        (free ?a - arm)
        (in-arm ?i - item ?a - arm)
        (in-tray ?i - item ?b - bot)
        (mount ?a - arm ?b - bot)
    )
    (:functions
        (bot-at ?b - bot)       - room
        (doors ?x - room)       - roomset
        (current_load ?b - bot) - load
        (weight ?i - item)      - load
        (load_limit ?b - bot)   - load
    )

    (:action move
        :parameters (?b - bot ?x ?y - room)
        :precondition (and (= (bot-at ?b) ?x) (member ?y (doors ?x)))
        :effect (assign (bot-at ?b) ?y)
    )
    (:action pick
        :parameters (?i - item ?x - room ?a - arm ?b - bot)
        :precondition (and (at ?i ?x) (= (bot-at ?b) ?x) (free ?a) (mount ?a ?b)
                           (<= (+ (current_load ?b) (weight ?i)) (load_limit ?b)))
        :effect (and (in-arm ?i ?a) (not (at ?i ?x)) (not (free ?a))
                     (increase (current_load ?b) (weight ?i)))
    )
    (:action drop
        :parameters (?i - item ?x - room ?a - arm ?b - bot)
        :precondition (and (in-arm ?i ?a) (= (bot-at ?b) ?x) (mount ?a ?b))
        :effect (and (free ?a) (at ?i ?x) (not (in-arm ?i ?a))
                     (decrease (current_load ?b) (weight ?i)))
    )
    (:action to-tray
        :parameters (?i - item ?a - arm ?b - bot)
        :precondition (and (in-arm ?i ?a) (mount ?a ?b))
        :effect (and (free ?a) (not (in-arm ?i ?a)) (in-tray ?i ?b))
    )
    (:action from-tray
        :parameters (?i - item ?a - arm ?b - bot)
        :precondition (and (in-tray ?i ?b) (mount ?a ?b) (free ?a))
        :effect (and (not (free ?a)) (in-arm ?i ?a) (not (in-tray ?i ?b)))
    )
)
