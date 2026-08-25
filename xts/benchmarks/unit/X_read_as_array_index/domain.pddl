;; BREAK TARGET: using the result of one array read as the index into another.
;;
;; `(read (outer) (read (inner) ?i) ?j)` — the row index is not a constant or
;; action parameter but the dynamic result of reading from (inner).
;;
;; The spec (pitfalls table) says:
;;   "Fluent-valued index (read (arr) (head)) — Not supported"
;; A nested read is the same violation: the index value is only known at
;; runtime, not at grounding time.
;;
;; This is a more subtle variant than X_fluent_as_array_index because the
;; index comes from a read expression rather than a plain scalar fluent —
;; it could slip through a simpler check that only looks for function names.
;;
;; Expected: semantic/compilation error for dynamic (read-valued) index.

(define (domain X-read-as-array-index)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        idx  - (number 0 2)
        val  - (number 0 9)
        idx_arr - (array 3 idx)   ; stores index values
        val_arr - (array 3 val)   ; stores data values
    )

    (:functions
        (pointers) - idx_arr   ; array of index values
        (data)     - val_arr   ; data array
        (result)   - val
    )

    ;; indirect lookup: data[pointers[?i]] — must be rejected
    (:action indirect_read
        :parameters (?i - idx)
        :precondition ()
        :effect (assign (result) (read (data) (read (pointers) ?i)))
    )
)
