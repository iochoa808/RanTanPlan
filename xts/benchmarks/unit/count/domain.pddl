(define (domain party-lights)

    (:requirements :typing :equality)

    (:types light - object)

    (:predicates
        (lit ?l - light)
        (party-on)
    )

    ;; Turn on a light that is currently off
    (:action turn-on
        :parameters (?l - light)
        :precondition (not (lit ?l))
        :effect (lit ?l)
    )

    ;; Turn off a light that is currently on
    (:action turn-off
        :parameters (?l - light)
        :precondition (lit ?l)
        :effect (not (lit ?l))
    )

    ;; Start the party when at least 2 of the 3 chosen lights are on.
    ;; Distinctness conditions prevent the planner from passing the same
    ;; light twice to artificially inflate the count.
    (:action start-party
        :parameters (?a - light ?b - light ?c - light)
        :precondition (and
            (not (= ?a ?b))
            (not (= ?a ?c))
            (not (= ?b ?c))
            (>= (count (lit ?a) (lit ?b) (lit ?c)) 2)
        )
        :effect (party-on)
    )
)
