;; SEMANTIC BREAK: forall effect whose arithmetic element expression provably
;; exceeds the declared element type's upper bound.
;;
;; `chain` is (set val) where val = (number 0 9).
;; The action uses (forall (?i - (number 8 9)) (add (+ ?i 1) (chain))).
;;
;; The forall range variable ?i has type int[8, 9], so (+ ?i 1) has the static
;; interval [8+1, 9+1] = [9, 10].  The upper bound 10 exceeds val's hi = 9.
;;
;; Unlike X_sem_arith_elem_exceeds_type (which uses an action parameter and is
;; only caught by unsolvability), forall range variables carry statically known
;; bounds — the overflow is detected at model-building / parse time.
;;
;; Expected: UPTypeError — upper bound 10 exceeds declared element type bound 9.

(define (domain X-sem-forall-set-oob)
    (:requirements :sets :bounded-integers :adl)

    (:types
        val    - (number 0 9)
        top    - (number 8 9)     ; forall range: only 8 and 9
        valset - (set val)
    )

    (:functions
        (chain) - valset
    )

    ;; forall ?i in [8,9]: add ?i+1 — when ?i=9, ?i+1=10 which is > hi=9
    (:action bulk_step
        :parameters ()
        :precondition ()
        :effect (forall (?i - top) (add (+ ?i 1) (chain)))
    )
)
