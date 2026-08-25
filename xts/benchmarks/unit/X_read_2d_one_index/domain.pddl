;; BREAK TARGET: reading a 2D array with only one index argument instead
;; of two, and writing a 2D array with the 1D syntax.
;;
;; `grid` is (array 3 3 val) — a 3×3 matrix requiring two indices.
;; `single_read` uses (read (grid) ?i) — only one index provided.
;;   This produces a partial application of the array, not a scalar value,
;;   and must be rejected rather than silently using ?j=0.
;;
;; `single_write` uses the 1D write syntax (write (grid) (?i) v) — one
;;   index in parens where the 2D form (write ((grid) ?i ?j) v) is required.
;;   This must be rejected as an arity mismatch.
;;
;; Expected: arity/dimension error on both the read and the write.

(define (domain X-read-2d-one-index)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        idx  - (number 0 2)
        val  - (number 0 9)
        grid - (array 3 3 val)
    )

    (:functions
        (board)  - grid
        (result) - val
    )

    ;; 2D read with only 1 index — must be rejected
    (:action single_read
        :parameters (?i - idx)
        :precondition ()
        :effect (assign (result) (read (board) ?i))
    )

    ;; 1D write syntax on a 2D array — must be rejected
    (:action single_write
        :parameters (?i - idx)
        :precondition ()
        :effect (write (board) (?i) 9)
    )
)
