(define (domain dual-gate)
    (:requirements :typing :adl :numeric-fluents :bounded-integers)

    (:types val - (number 0 9))

    (:predicates
        (flag-a)
        (flag-b)
    )

    (:functions
        (cell-a) - val
        (cell-b) - val
    )

    ;; Enable the second gate.
    (:action activate
        :parameters ()
        :precondition (not (flag-b))
        :effect (flag-b)
    )

    ;; Two independent conditional effects in one action.
    (:action process
        :parameters ()
        :effect (and
            (when (flag-a) (assign (cell-a) 0))
            (when (flag-b) (assign (cell-b) 0))
        )
    )
)
