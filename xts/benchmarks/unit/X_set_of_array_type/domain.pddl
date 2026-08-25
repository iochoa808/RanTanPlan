(define (domain x-set-of-array)
    (:requirements :sets :arrays :bounded-integers :typing)

    (:types
        val     - (number 0 4)
        arr     - (array 3 val)
        ;; Error: set whose element type is an array — not supported
        arr-set - (set arr)
    )

    (:predicates (done))
    (:functions  (arrays) - arr-set)

    (:action check
        :parameters ()
        :precondition (not (done))
        :effect (done)
    )
)
