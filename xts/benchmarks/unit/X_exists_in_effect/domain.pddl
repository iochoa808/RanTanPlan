;; BREAK TARGET: `exists` quantifier used inside a :effect body.
;;
;; The spec (section 4.5 and pitfalls table) states:
;;   "`exists` is supported in preconditions only — it cannot appear in effects."
;;
;; The action `pick_high` wraps an effect inside an (exists ...) — this should
;; be rejected rather than silently treated as a forall or no-op.
;;
;; Expected: semantic/compilation error on the :effect of pick_high.

(define (domain X-exists-in-effect)
    (:requirements :typing :adl :arrays :bounded-integers)

    (:types
        idx - (number 0 3)
        val - (number 0 9)
        arr - (array 4 val)
    )

    (:predicates
        (high_found)
    )

    (:functions
        (cells) - arr
    )

    ;; exists in effect — must be rejected
    (:action pick_high
        :parameters ()
        :precondition (not (high_found))
        :effect (exists (?i - (number 0 3))
                    (when (> (read (cells) ?i) 7)
                        (high_found)))
    )
)
