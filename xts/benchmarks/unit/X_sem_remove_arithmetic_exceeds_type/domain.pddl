(define (domain x-remove-arith-exceeds)
    (:requirements :sets :bounded-integers :typing)

    (:types
        ;; parameter range [7,9]; element type [0,9]
        ;; remove (n+3): when n=7 → removes 10, which exceeds element type
        param-t - (number 7 9)
        elem-t  - (number 0 9)
        bagtype - (set elem-t)
    )

    (:predicates (done))
    (:functions  (bag) - bagtype)

    ;; Error: remove (n+3) where n ∈ [7,9] — n+3 ∈ [10,12], all exceed elem-t [0,9]
    (:action bad-remove
        :parameters (?n - param-t)
        :precondition (member ?n (bag))
        :effect (and
            (remove (+ ?n 3) (bag))
            (done)
        )
    )
)
