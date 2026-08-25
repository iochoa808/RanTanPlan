(define (domain x-read-3d-partial)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        d-idx  - (number 0 1)
        r-idx  - (number 0 1)
        val    - (number 0 9)
        tensor - (array 2 2 2 val)
    )

    (:functions
        (tensor) - tensor
        (result) - val
    )

    ;; Error: tensor[d][r] is a 1D-array sub-expression, not a scalar.
    ;; Assigning it to (result) — a scalar val — is a type error.
    (:action partial-read
        :parameters (?d - d-idx ?r - r-idx)
        :effect (assign (result) (read (tensor) ?d ?r))
    )
)
