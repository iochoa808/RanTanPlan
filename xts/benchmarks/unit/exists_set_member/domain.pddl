(define (domain sensor)
    (:requirements :adl :sets :bounded-integers :typing)

    (:types
        val    - (number 0 9)
        valset - (set val)
    )

    (:predicates (detected))
    (:functions  (bag) - valset)

    ;; Add the value 7 to the bag.
    (:action fill
        :parameters ()
        :precondition (not (member 7 (bag)))
        :effect (add 7 (bag))
    )

    ;; Fire when some integer in [3..7] is in the bag.
    (:action detect
        :parameters ()
        :precondition (and
            (not (detected))
            (exists (?i - (number 3 7)) (member ?i (bag)))
        )
        :effect (detected)
    )
)
