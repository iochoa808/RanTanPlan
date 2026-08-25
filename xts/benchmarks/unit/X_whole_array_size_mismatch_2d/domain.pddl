;; BREAK TARGET: array.mk with wrong row or column count for a 2D array in an effect.
;;
;; `grid` is (array 2 3 val) — a 2×3 matrix.
;; `load_wrong_cols` assigns with only 2 columns per row instead of 3:
;;   (assign (grid) (array.mk (1 2) (3 4)))  — 2 cols, should be 3.
;; `load_wrong_rows` assigns with 3 rows instead of 2:
;;   (assign (grid) (array.mk (1 2 3) (4 5 6) (7 8 9)))  — 3 rows, should be 2.
;;
;; X_array_mk_overcount covers init-time mismatch.
;; This covers the effect-time mismatch for a 2D array.
;;
;; Expected: semantic error — array.mk shape does not match declared type.

(define (domain X-size-mismatch-2d)
    (:requirements :typing :arrays :bounded-integers)

    (:types
        val    - (number 0 9)
        grid-t - (array 2 3 val)
    )

    (:functions
        (grid) - grid-t
    )

    ;; Error: 2 columns per row instead of 3
    (:action load_wrong_cols
        :parameters ()
        :precondition ()
        :effect (assign (grid) (array.mk (1 2) (3 4)))
    )

    ;; Error: 3 rows instead of 2
    (:action load_wrong_rows
        :parameters ()
        :precondition ()
        :effect (assign (grid) (array.mk (1 2 3) (4 5 6) (7 8 9)))
    )
)
