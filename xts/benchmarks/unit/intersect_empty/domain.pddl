;; Test: INTERSECTION OF DISJOINT SETS — fires only when disjoint, sets result.
;;
;; KEY PATTERN: (disjoint A B) as precondition gates an action;
;;   (intersect A B) effect assigned to result when sets share no elements.
;;   sets2 tests intersection with overlapping sets (non-empty result).
;;   This verifies the disjoint-check + intersect-effect path together.
;;
;; Domain: a and b are disjoint sets; compute fires only when disjoint.
;; Initial: a={1,3}, b={2,4}, result={}.
;; Goal: (computed)  — flag set by compute to confirm action fired.
;; Plan: compute()  — 1 step.

(define (domain intersect-empty)
    (:requirements :sets :bounded-integers :typing)

    (:types
        val    - (number 1 9)
        valset - (set val)
    )

    (:predicates (computed))

    (:functions
        (a)      - valset
        (b)      - valset
        (result) - valset
    )

    ;; Fires only when a and b are disjoint; records result = a ∩ b (= empty).
    (:action compute
        :parameters ()
        :precondition (disjoint (a) (b))
        :effect (and (assign (result) (intersect (a) (b)))
                     (computed))
    )
)
