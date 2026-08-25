(define (domain pancake-int)

    (:requirements :typing :fluents :bounded-integers)

    (:types
        idx - (number 0 4)
    )

    (:functions
        (val ?i - idx) - idx
    )

    (:action flip
        :parameters (?f - idx)
        :effect (and
            (forall (?i - (number 0 ?f))
                (assign (val ?i) (val (- ?f ?i)))
            )
        )
    )
)
