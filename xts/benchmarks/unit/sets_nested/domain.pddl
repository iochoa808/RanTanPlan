(define (domain set-nested-effects)

    (:requirements :sets)

    (:types
        level    - (number 0 5)
        levelset - (set level)
    )

    (:functions
        (bucket_a) - levelset
        (bucket_b) - levelset
        (bucket_c) - levelset
        (result)   - levelset
    )

    ; Gap 4: (assign result (union (union bucket_a bucket_b) bucket_c))
    ; The inner (union bucket_a bucket_b) is a nested set expression — not a
    ; direct STATE_VARIABLE.  Previously silently dropped; now handled.
    (:action merge_all
        :parameters ()
        :precondition ()
        :effect (assign (result) (union (union (bucket_a) (bucket_b)) (bucket_c)))
    )

    ; Ordinary add for setup.
    (:action add_to_a
        :parameters (?x - level)
        :precondition (not (member ?x (bucket_a)))
        :effect (add ?x (bucket_a))
    )

    (:action add_to_b
        :parameters (?x - level)
        :precondition (not (member ?x (bucket_b)))
        :effect (add ?x (bucket_b))
    )

    (:action add_to_c
        :parameters (?x - level)
        :precondition (not (member ?x (bucket_c)))
        :effect (add ?x (bucket_c))
    )
)
