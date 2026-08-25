;; SEMANTIC BREAK: (cardinality f) where f is a SCALAR bounded-integer fluent,
;; not a set-typed fluent.
;;
;; `cardinality` is defined for SET types only — it returns the number of
;; elements currently in the set.  Applying it to a plain numeric fluent
;; is a type error: a number has no "cardinality" in any meaningful sense.
;;
;; SEMANTIC RULE: The argument of `cardinality` must have a set type.
;; `(number 0 9)` is a bounded integer, not a set — `(cardinality (score))`
;; should be rejected as a type mismatch.
;;
;; This is a pure semantic check that does not require runtime analysis:
;; the type of (score) is statically known to be (number 0 9), not a set.
;;
;; Two variants:
;;   - `check_big`: uses `cardinality` in a precondition comparison
;;   - `award`:     uses `cardinality` as an arithmetic operand in assign
;;     The second variant additionally tests whether cardinality can be
;;     embedded inside arithmetic (also wrong).
;;
;; Expected: type error for both actions.

(define (domain X-sem-cardinality-of-scalar)
    (:requirements :typing :numeric-fluents :bounded-integers :sets)

    (:types
        score - (number 0 9)
    )

    (:functions
        (score) - score     ; scalar bounded integer — NOT a set
        (bonus) - score
    )

    ;; cardinality applied to a scalar in precondition
    (:action check_big
        :parameters ()
        :precondition (>= (cardinality (score)) 3)
        :effect (assign (bonus) 1)
    )

    ;; cardinality embedded in arithmetic assign
    (:action award
        :parameters ()
        :precondition ()
        :effect (assign (bonus) (+ (cardinality (score)) 1))
    )
)
