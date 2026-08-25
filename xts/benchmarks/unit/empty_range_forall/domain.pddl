(define (domain empty-range-forall)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx - (number 0 9)
        val - (number 0 9)
        arr - (array 4 val)
    )

    (:predicates (done))
    (:functions (cells) - arr)

    ;; noop_sweep: forall over empty range (5..3) — precondition vacuously true,
    ;; effect is a no-op.  Cells must NOT be altered.
    (:action noop_sweep
        :parameters ()
        :precondition (forall (?i - (number 5 3)) (< (read (cells) ?i) 0))
        :effect (and
            (forall (?i - (number 5 3)) (write (cells) (?i) 0))
            (done)
        )
    )
)