(define (domain set-constant-test)

    (:requirements :sets)

    (:types
        level   - (number 0 9)   ; bounded integer
        levelset - (set level)   ; set of levels
    )

    (:functions
        (active)  - levelset   ; currently active levels
        (allowed) - levelset   ; reference set
    )

    ; Gap 2a: (member ?a (set.mk (2 4 6))) — SET_CONSTANT with integer elements.
    ; Can only activate even levels.
    (:action activate_even
        :parameters (?a - level)
        :precondition (and
            (member ?a (set.mk (2 4 6)))
            (not (member ?a (active))))
        :effect (add ?a (active))
    )

    ; Ordinary remove.
    (:action deactivate
        :parameters (?a - level)
        :precondition (member ?a (active))
        :effect (remove ?a (active))
    )

    ; Gap 3: (cardinality (set.mk (2 4 6))) = 3.
    ; Fires when active has at least 3 elements (same count as the literal).
    (:action mark_complete
        :parameters ()
        :precondition (>= (cardinality (active)) (cardinality (set.mk (2 4 6))))
        :effect (assign (active) (union (active) (allowed)))
    )
)
