;; SEMANTIC BREAK: forall effect whose arithmetic element expression provably
;; falls BELOW the declared element type's lower bound.
;;
;; `zone` is (set slot) where slot = (number 2 9), so valid range is [2, 9].
;; The action uses (forall (?i - (number 2 3)) (add (- ?i 1) (zone))).
;;
;; The forall range variable ?i has type int[2, 3], so (- ?i 1) has the static
;; interval [2-1, 3-1] = [1, 2].  The lower bound 1 is below slot's lo = 2.
;;
;; This complements X_sem_forall_set_oob which tests the upper-bound direction.
;;
;; Expected: UPTypeError — lower bound 1 is below declared element type bound 2.

(define (domain X-sem-forall-set-lb-oob)
    (:requirements :sets :bounded-integers :adl)

    (:types
        slot    - (number 2 9)    ; lower bound is 2
        low     - (number 2 3)    ; forall range starts at 2, so ?i-1 reaches 1
        slotset - (set slot)
    )

    (:functions
        (zone) - slotset
    )

    ;; forall ?i in [2,3]: add ?i-1 — when ?i=2, ?i-1=1 which is < lo=2
    (:action bulk_drop
        :parameters ()
        :precondition ()
        :effect (forall (?i - low) (add (- ?i 1) (zone)))
    )
)
