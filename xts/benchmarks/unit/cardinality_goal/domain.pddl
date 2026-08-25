(define (domain exact-bag)
    (:requirements :sets :bounded-integers :typing)

    (:types
        val    - (number 0 5)
        valset - (set val)
    )

    (:functions (bag) - valset)

    ;; Add element n if bag has fewer than 3 elements and n is absent.
    (:action fill
        :parameters (?n - val)
        :precondition (and
            (not (member ?n (bag)))
            (< (cardinality (bag)) 3)
        )
        :effect (add ?n (bag))
    )
)
