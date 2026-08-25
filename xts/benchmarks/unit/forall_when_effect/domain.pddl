(define (domain sweep)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx    - (number 0 3)
        val    - (number 0 9)
        flag   - (number 0 1)
        arr    - (array 4 val)
        flagarr - (array 4 flag)
    )

    (:functions
        (cells)  - arr
        (active) - flagarr
    )

    ;; Mark cell i as active.
    (:action mark
        :parameters (?i - idx)
        :precondition (= (read (active) ?i) 0)
        :effect (write (active) (?i) 1)
    )

    ;; Zero every cell whose active flag is 1.
    ;; forall body: when active[i]=1 → cells[i]=0
    (:action sweep
        :parameters ()
        :effect (forall (?i - (number 0 3))
            (when (= (read (active) ?i) 1)
                (write (cells) (?i) 0)))
    )
)
