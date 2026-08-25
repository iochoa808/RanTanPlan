;; SEMANTIC BREAK: constant integer used as set element in an effect exceeds
;; the declared element type's upper bound.
;;
;; `chain` is (set val) where val = (number 0 9), so the valid range is [0, 9].
;; The action `overflow` tries to add the literal 10 to `chain`.
;;
;; The static element interval for the constant 10 is [10, 10].
;; Since 10 > hi = 9, this is provably out of range.
;;
;; This check fires at model-building time (PDDL parse / add_effect), before
;; any search is attempted.
;;
;; Expected: UPTypeError — set element constant 10 exceeds declared type bound 9.

(define (domain X-sem-const-set-oob)
    (:requirements :sets :bounded-integers)

    (:types
        val    - (number 0 9)
        valset - (set val)
    )

    (:functions
        (chain) - valset
    )

    ;; literal 10 is above val's upper bound of 9
    (:action overflow
        :parameters ()
        :precondition ()
        :effect (add 10 (chain))
    )
)
