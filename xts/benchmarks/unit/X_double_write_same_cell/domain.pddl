;; BREAK TARGET: two conflicting writes to the same array cell within a
;; single action's effect conjunction.
;;
;; Standard PDDL treats effects as applied simultaneously to the pre-action
;; state (the "parallel semantics" of ADL).  With two writes to the same
;; cell, the resulting state is ambiguous — or defined only by coincidental
;; evaluation order in the encoder.
;;
;; Case 1 — `overwrite_const`: both writes use constant values (3 then 7).
;;   The compiler must decide which one wins; the result is non-deterministic
;;   with respect to the language spec.
;;
;; Case 2 — `overwrite_read_arith`: both writes use arithmetic on the
;;   PRE-action cell value.  If the encoder evaluates writes sequentially
;;   (using the intermediate post-write value for the second read), it
;;   produces a different result than if both reads use the original value.
;;   This exposes an order-dependency bug in read-modify-write chaining.
;;   E.g., cells[0]=5: first write sets it to (5+1)=6, second to (5+2)=7.
;;   But if the second read sees 6 (already written), it sets to (6+2)=8.
;;
;; Expected: error for conflicting writes to the same cell, OR at minimum
;; a deterministic "last-write wins with pre-state reads" guarantee.

(define (domain X-double-write-same-cell)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        slot  - (number 0 2)
        val   - (number 0 9)
        store - (array 3 val)
    )

    (:functions
        (cells) - store
    )

    ;; two constant writes to cell 0 — order-dependent result
    (:action overwrite_const
        :parameters ()
        :precondition ()
        :effect (and
            (write (cells) (0) 3)
            (write (cells) (0) 7)
        )
    )

    ;; two read-modify-write effects on cell 1 — ambiguous if reads see
    ;; intermediate state vs pre-action state
    (:action overwrite_read_arith
        :parameters ()
        :precondition (< (read (cells) 1) 7)
        :effect (and
            (write (cells) (1) (+ (read (cells) 1) 1))
            (write (cells) (1) (+ (read (cells) 1) 2))
        )
    )
)
