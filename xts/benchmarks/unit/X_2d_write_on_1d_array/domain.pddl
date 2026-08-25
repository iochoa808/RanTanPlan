;; BREAK TARGET: 2D write syntax applied to a 1D array, and 1D write syntax
;; applied to a 2D array — dimension arity mismatch in write effects.
;;
;; The spec (section 2.6) defines:
;;   1D write: (write (fluent) (?i) new-value)
;;   2D write: (write ((fluent) row col) new-value)
;;
;; `line` is a 1D array.  `fill_2d` applies the 2D double-paren syntax to
;; it with two index arguments — the parser/compiler must reject this because
;; a 1D array has exactly one dimension.
;;
;; Conversely, `board` is a 2D array.  `fill_1d` applies the 1D single-paren
;; index syntax with only one index — also wrong dimension.
;;
;; Expected: arity/dimension error on both write effects.

(define (domain X-2d-write-on-1d-array)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        idx - (number 0 2)
        val - (number 0 9)

        line  - (array 3 val)      ; 1D
        board - (array 3 3 val)    ; 2D
    )

    (:functions
        (row)   - line
        (grid)  - board
    )

    ;; 2D syntax on a 1D array — must be rejected
    (:action fill_2d
        :parameters (?i ?j - idx)
        :precondition ()
        :effect (write ((row) ?i ?j) 7)
    )

    ;; 1D syntax on a 2D array — must be rejected
    (:action fill_1d
        :parameters (?i - idx)
        :precondition ()
        :effect (write (grid) (?i) 5)
    )
)
