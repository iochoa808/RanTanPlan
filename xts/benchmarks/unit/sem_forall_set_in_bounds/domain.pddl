;; TEST: forall effect with arithmetic set element that stays within type bounds.
;;
;; This is the VALID counterpart to X_sem_forall_set_oob.  The forall range
;; is [8, 8] (a single value), so (+ ?i 1) = 9, which fits within val's
;; range [0, 9].  No error should be raised at parse time or solve time.
;;
;; The single-element forall avoids the multi-effect conflict that arises when
;; the same set fluent appears as the target of many simultaneous add effects.
;;
;; Domain: start with {0}; one action adds element 9 via a one-iteration forall.

(define (domain sem-forall-set-in-bounds)
    (:requirements :sets :bounded-integers :adl)

    (:types
        val    - (number 0 9)
        valset - (set val)
    )

    (:functions
        (chain) - valset
    )

    ;; Forall range [8, 8] → ?i is always 8 → (+ ?i 1) = 9, which is <= 9.
    ;; Static interval [9, 9] is within [0, 9]: the check must NOT fire.
    (:action add_top
        :parameters ()
        :precondition (not (member 9 (chain)))
        :effect (forall (?i - (number 8 8)) (add (+ ?i 1) (chain)))
    )
)
