(define (domain full-bag)
    (:requirements :adl :sets :bounded-integers :typing)

    (:types
        val    - (number 0 4)
        valset - (set val)
    )

    (:predicates (verified))
    (:functions  (bag) - valset)

    ;; Add element n to bag if absent.
    (:action fill
        :parameters (?n - val)
        :precondition (not (member ?n (bag)))
        :effect (add ?n (bag))
    )

    ;; Fire only when every integer in [0..4] is in the bag.
    (:action verify
        :parameters ()
        :precondition (and
            (not (verified))
            (forall (?i - (number 0 4)) (member ?i (bag)))
        )
        :effect (verified)
    )
)
