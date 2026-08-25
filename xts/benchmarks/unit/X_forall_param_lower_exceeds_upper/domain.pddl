;; BREAK TARGET: FORALL with PARAM LOWER BOUND that ALWAYS EXCEEDS the upper bound.
;;
;; `?lo` is of type (number 4 5) — so ?lo is always ≥ 4.
;; The forall range is (number ?lo 3) — upper bound is the constant 3.
;; Since lo ∈ {4, 5} and hi = 3, the range is always inverted (lo > hi).
;;
;; The forall effect thus never fires for any grounding of ?lo.
;; `cells` is (array 4 val); the goal requires cells[0]=1 which cannot be
;; achieved because zero_cells never writes anything → UNSOLVABLE.
;;
;; Expected: UNSOLVABLE — the always-empty forall makes the goal unreachable.

(define (domain X-forall-lb-gt-ub)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        lo-t  - (number 4 5)   ;; lower bound: always ≥ 4
        val   - (number 0 1)
        arr-t - (array 4 val)
    )

    (:functions
        (cells) - arr-t
    )

    ;; forall range (lo..3) is always empty because lo ∈ {4,5} > 3.
    ;; No write ever fires, so cells stays all-zero.
    (:action zero_cells
        :parameters (?lo - lo-t)
        :precondition ()
        :effect (forall (?i - (number ?lo 3))
                    (write (cells) (?i) 1))
    )
)
