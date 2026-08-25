;; BREAK TARGET: (member ?x (arr_fluent)) — applying set membership to an
;; array-typed fluent instead of a set-typed fluent.
;;
;; `(cells)` has return type `store` which is (array 4 val), not a set type.
;; The `member` predicate requires its second argument to be a set fluent.
;; Passing an array should be rejected as a type mismatch.
;;
;; Expected: semantic error — `member` applied to a non-set fluent.

(define (domain X-member-on-array)
    (:requirements :typing :arrays :bounded-integers :sets)

    (:types
        slot  - (number 0 3)
        val   - (number 0 9)
        store - (array 4 val)    ; array, NOT a set
    )

    (:functions
        (cells) - store
    )

    ;; (member ?v (cells)) — second arg is array, must be rejected
    (:action find_val
        :parameters (?v - val)
        :precondition (member ?v (cells))
        :effect ()
    )
)
