;; BREAK TARGET: writing a SET-VALUED expression into an array cell whose
;;               declared element type is a BOUNDED INTEGER.
;;
;; `cells` is (array 3 val) where val = (number 0 9) — integer element type.
;; `bag`   is (set val) — a set of integers.
;;
;; `overwrite` assigns the SET FLUENT (bag) into cells[0], which expects an
;; integer, not a set.  This is a type mismatch: set ≠ bounded int.
;;
;; Expected: semantic/type error — set-valued expression in an integer cell write.

(define (domain X-set-into-int)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        val    - (number 0 9)
        valset - (set val)
        arr-t  - (array 3 val)
    )

    (:functions
        (cells) - arr-t
        (bag)   - valset
    )

    ;; Error: (bag) is a set; cells[0] expects an integer.
    (:action overwrite
        :parameters ()
        :precondition ()
        :effect (write (cells) (0) (bag))
    )
)
