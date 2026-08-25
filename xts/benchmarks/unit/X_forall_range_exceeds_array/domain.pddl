;; BREAK TARGET: forall loop whose index range extends past the array's
;; last valid index.
;;
;; `cells` is (array 4 val) — valid indices are 0..3.
;; `zero_all` uses a forall over (number 0 7), which generates write effects
;; for indices 0, 1, 2, 3, 4, 5, 6, 7.  Indices 4–7 are all out of bounds.
;;
;; A checker that validates only "is the index type's range consistent with
;; the array size" on scalar reads/writes may miss this because the loop
;; bound is a literal in the forall type expression, not an action parameter
;; or an index-typed variable — it is never cross-checked against array size.
;;
;; Expected: error for the forall range [4..7] accessing a size-4 array.

(define (domain X-forall-range-exceeds-array)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        slot  - (number 0 3)
        val   - (number 0 9)
        store - (array 4 val)   ; size 4, valid indices 0..3
    )

    (:functions
        (cells) - store
    )

    ;; forall range [0..7] writes indices 4..7 which are OOB
    (:action zero_all
        :parameters ()
        :precondition ()
        :effect (forall (?i - (number 0 7))
                    (write (cells) (?i) 0))
    )
)
