(define (domain visit-all-sets)
    (:requirements :typing :sets :object-fluents)

    (:types
        place    - object
        placeset - (set place)
    )

    (:functions
        (robot_at)            - place
        (connects ?x - place) - placeset    ;; static adjacency
        (visited)             - placeset
    )

    (:action move
        :parameters (?curpos ?nextpos - place)
        :precondition (and (= (robot_at) ?curpos)
                           (member ?nextpos (connects ?curpos)))
        :effect (and (assign (robot_at) ?nextpos)
                     (add ?nextpos (visited)))
    )
)