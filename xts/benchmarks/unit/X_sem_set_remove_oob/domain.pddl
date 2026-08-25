;; SEMANTIC BREAK: constant integer used as the element argument of a SET_REMOVE
;; effect exceeds the declared element type's upper bound.
;;
;; The range check applies equally to `add` and `remove`: an element expression
;; whose static interval falls outside the type's declared range is always
;; an error, whether you are adding or removing it.
;;
;; `chain` is (set val) where val = (number 0 9).
;; The action `bad_remove` calls (remove 10 (chain)) — 10 > hi = 9.
;;
;; Expected: UPTypeError — set element constant 10 exceeds declared type bound 9
;; (detected at model-building / parse time, not at search time).

(define (domain X-sem-set-remove-oob)
    (:requirements :sets :bounded-integers)

    (:types
        val    - (number 0 9)
        valset - (set val)
    )

    (:functions
        (chain) - valset
    )

    ;; Removing 10 from a set whose element type only goes up to 9 is a
    ;; static range violation regardless of whether 10 is actually present.
    (:action bad_remove
        :parameters ()
        :precondition ()
        :effect (remove 10 (chain))
    )
)
