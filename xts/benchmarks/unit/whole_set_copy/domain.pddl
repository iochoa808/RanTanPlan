(define (domain archive)
    (:requirements :sets :bounded-integers :typing)

    (:types
        val    - (number 0 4)
        valset - (set val)
    )

    (:functions
        (original) - valset
        (backup)   - valset
    )

    ;; Copy original → backup (whole-set assignment from another fluent).
    (:action archive
        :parameters ()
        :precondition (= (cardinality (backup)) 0)
        :effect (assign (backup) (original))
    )

    ;; Clear original to empty set.
    (:action flush
        :parameters ()
        :precondition (>= (cardinality (backup)) 1)
        :effect (assign (original) (difference (original) (original)))
    )
)
