;; SEMANTIC BREAK: constant integer used as set element in an effect falls
;; BELOW the declared element type's lower bound.
;;
;; `restricted` is (set slot) where slot = (number 5 9), so valid range is [5, 9].
;; The action `underflow` tries to add the literal 4 — below the lower bound.
;;
;; The static element interval for the constant 4 is [4, 4].
;; Since 4 < lo = 5, this is provably out of range.
;;
;; This complements X_sem_const_set_oob which tests the upper-bound direction.
;;
;; Expected: UPTypeError — set element constant 4 is below declared type bound 5.

(define (domain X-sem-const-set-lb-oob)
    (:requirements :sets :bounded-integers)

    (:types
        slot    - (number 5 9)     ; lower bound is 5, not 0
        slotset - (set slot)
    )

    (:functions
        (restricted) - slotset
    )

    ;; literal 4 is below slot's lower bound of 5
    (:action underflow
        :parameters ()
        :precondition ()
        :effect (add 4 (restricted))
    )
)
