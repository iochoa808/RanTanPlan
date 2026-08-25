;; BREAK TARGET: set operations (subset, union, difference, disjoint)
;; applied to two set fluents whose element types are DIFFERENT.
;;
;; `int_bag`  is (set level)  where level is (number 0 5)
;; `obj_bag`  is (set item)   where item  is an object type
;;
;; Applying `subset`, `union`, `difference`, or `disjoint` to operands of
;; incompatible element types is a type error.  A checker that only verifies
;; "both arguments are set types" without comparing their element types will
;; silently accept these as valid.
;;
;; This is subtle because:
;;   - Both arguments ARE set-typed (not scalars or arrays).
;;   - The error is in the ELEMENT TYPE mismatch, one level deeper.
;;   - At the bit-vector encoding level both types map to bit-vectors and
;;     could be silently ORed together with meaningless results.
;;
;; Expected: type error for each cross-type set operation.

(define (domain X-set-ops-type-mismatch)
    (:requirements :typing :sets :bounded-integers)

    (:types
        item    - object
        itemset - (set item)

        level   - (number 0 5)
        levelset - (set level)
    )

    (:constants item_a item_b - item)

    (:functions
        (obj_bag) - itemset
        (int_bag) - levelset
        (result_obj) - itemset
        (result_int) - levelset
    )

    ;; subset across incompatible element types
    (:action check_cross_subset
        :parameters ()
        :precondition (subset (obj_bag) (int_bag))
        :effect ()
    )

    ;; union of incompatible element types
    (:action merge_cross
        :parameters ()
        :precondition ()
        :effect (assign (result_obj) (union (obj_bag) (int_bag)))
    )

    ;; difference of incompatible element types
    (:action diff_cross
        :parameters ()
        :precondition ()
        :effect (assign (result_int) (difference (int_bag) (obj_bag)))
    )

    ;; disjoint check across incompatible element types
    (:action check_disjoint_cross
        :parameters ()
        :precondition (disjoint (obj_bag) (int_bag))
        :effect ()
    )
)
