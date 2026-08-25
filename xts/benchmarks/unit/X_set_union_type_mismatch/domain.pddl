;; SEMANTIC BREAK: set ALGEBRA (union) between sets with incompatible element
;; types — one holds user objects, the other bounded integers.
;;
;; X_set_ops_type_mismatch covers `subset` (a boolean set PREDICATE).  This
;; covers `union` (a set-VALUED operation in an effect), a different code
;; path: the result type must be checked against the destination fluent's
;; element type.  A checker that only verifies "all operands are set types"
;; without comparing element types would silently accept this and then
;; encode an aliased/garbage set.
;;
;; Expected: type error — union of set{item} and set{integer[0,5]} is ill-typed.

(define (domain X-set-union-type-mismatch)
    (:requirements :typing :sets :bounded-integers)

    (:types
        item   - object
        lvl    - (number 0 5)
        objset - (set item)
        intset - (set lvl)
    )

    (:functions
        (ob)  - objset
        (ib)  - intset
        (res) - objset
    )

    (:action bad
        :parameters ()
        :effect (assign (res) (union (ob) (ib)))
    )
)
