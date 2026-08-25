;; BREAK TARGET: declaring a set whose element type is itself a set type.
;;
;; The spec (section 6, nesting matrix) states:
;;   "(set (set bounded-int))" and "(set (set user-object))" are both
;;   NOT compilable.  A set type's element type must be a bounded integer
;;   or a user object — not another set.
;;
;; `powerset` is declared as (set tagset) where tagset is already (set tag).
;; Every operation that touches powerset should fail the type check.
;;
;; Expected: type error or compilation error at the :types declaration or
;;           when first compiling an action that uses (powerset).

(define (domain X-set-of-set-type)
    (:requirements :typing :sets)

    (:types
        tag      - object
        tagset   - (set tag)       ; valid: set of objects
        powerset - (set tagset)    ; INVALID: set of sets
    )

    (:constants red blue green - tag)

    (:functions
        (library) - powerset    ; fluent of the illegal nested-set type
    )

    ;; Any action using (library) should trigger the error
    (:action add_group
        :parameters (?g - tagset)
        :precondition (not (member ?g (library)))
        :effect (add ?g (library))
    )
)
