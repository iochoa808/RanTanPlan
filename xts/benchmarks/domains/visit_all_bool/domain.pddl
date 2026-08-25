(define (domain visit-all-bool)
    (:requirements :typing :object-fluents)

    (:types
        place - object
    )

    (:predicates
        (connects ?x - place ?y - place)    ;; static adjacency
        (visited  ?wp - place)
    )

    (:functions
        (robot_at) - place
    )

    (:action move
        :parameters (?curpos ?nextpos - place)
        :precondition (and (= (robot_at) ?curpos)
                           (connects ?curpos ?nextpos))
        :effect (and (assign (robot_at) ?nextpos)
                     (visited ?nextpos))
    )
)
