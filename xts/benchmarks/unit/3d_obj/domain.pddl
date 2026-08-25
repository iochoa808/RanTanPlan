(define (domain cube-swap)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        person - object
        d-idx  - (number 0 1)
        r-idx  - (number 0 1)
        cube   - (array 2 2 2 person)
    )

    (:functions (cube) - cube)

    ;; Swap the two persons in columns 0 and 1 of the same (depth, row).
    (:action swap-col
        :parameters (?d - d-idx ?r - r-idx ?pa - person ?pb - person)
        :precondition (and
            (= (read (cube) ?d ?r 0) ?pa)
            (= (read (cube) ?d ?r 1) ?pb)
        )
        :effect (and
            (write ((cube) ?d ?r 0) ?pb)
            (write ((cube) ?d ?r 1) ?pa)
        )
    )
)
