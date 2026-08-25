(define (domain threshold-stamp)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx - (number 0 3)
        val - (number 0 9)
        cnt - (number 0 9)
        arr - (array 4 val)
    )

    (:functions
        (cells)   - arr
        (counter) - cnt
    )

    ;; Increment counter by 1.
    (:action inc
        :parameters ()
        :precondition (< (counter) 9)
        :effect (assign (counter) (+ (counter) 1))
    )

    ;; Write 7 into cells[i] ONLY when counter >= 5 (numeric when-guard).
    (:action stamp
        :parameters (?i - idx)
        :precondition (= (read (cells) ?i) 0)
        :effect (when (>= (counter) 5)
            (write (cells) (?i) 7))
    )
)
