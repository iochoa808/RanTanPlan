;; Test: SUBSET and DISJOINT with an inline SET.MK LITERAL operand.
;;
;; KEY PATTERNS:
;;   (subset (basket) (set.mk (0 2 4)))   — subset check against a literal set
;;   (disjoint (bag) (set.mk (1 3)))      — disjoint check against a literal set
;;
;; subset/disjoint tests elsewhere compare two fluent-valued sets.
;; This exercises the mixed fluent vs. set.mk literal form.
;;
;; Domain: basket [0,4] set; approve only when basket ⊆ {0,2,4} AND
;;         the odd-check set is disjoint from {1,3}.
;; Initial: basket={0,2}, odds={}; goal: approved=1.
;; Plan: approve()  — 1 step.

(define (domain approval)
    (:requirements :sets :bounded-integers :typing)

    (:types
        val    - (number 0 4)
        valset - (set val)
        flag   - (number 0 1)
    )

    (:functions
        (basket)   - valset
        (odds)     - valset
        (approved) - flag
    )

    ;; Approve when basket ⊆ {0,2,4} AND odds is disjoint from {1,3}.
    (:action approve
        :parameters ()
        :precondition (and
            (= (approved) 0)
            (subset (basket) (set.mk (0 2 4)))
            (disjoint (odds) (set.mk (1 3)))
        )
        :effect (assign (approved) 1)
    )
)
