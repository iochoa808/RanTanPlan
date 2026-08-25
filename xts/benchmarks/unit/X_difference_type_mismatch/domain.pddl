(define (domain x-difference-type-mismatch)
    (:requirements :sets :bounded-integers :typing)

    (:types
        item    - object
        level   - (number 0 5)
        itemset - (set item)
        intset  - (set level)
    )

    (:predicates (done))
    (:functions
        (obj-bag) - itemset
        (int-bag) - intset
        (result)  - itemset
    )

    ;; Error: difference requires both operands to have the same element type.
    (:action bad
        :parameters ()
        :effect (and
            (assign (result) (difference (obj-bag) (int-bag)))
            (done)
        )
    )
)
