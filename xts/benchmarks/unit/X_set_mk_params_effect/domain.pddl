;; BREAK TARGET: (assign (basket) (set.mk (?a ?b))) in an effect.
;;
;; The spec (pitfalls table, section 7) explicitly states:
;;   "set.mk with parameter elements in effect (assign (f) (set.mk (?a ?b)))"
;;   is NOT supported.  set.mk in effects only accepts object constants.
;;
;; The action `reset_pair` tries to build a literal set from two action
;; parameters.  This should be rejected during compilation, not silently
;; produce a wrong (e.g., empty) set.
;;
;; Expected: semantic/compilation error on the :effect of reset_pair.

(define (domain X-set-mk-params-effect)
    (:requirements :sets)

    (:types
        item    - object
        itemset - (set item)
    )

    (:constants item_a item_b item_c item_d - item)

    (:functions
        (basket) - itemset
    )

    ;; This action should be illegal: set.mk cannot hold parameters
    (:action reset_pair
        :parameters (?a - item ?b - item)
        :precondition (and
            (member ?a (basket))
            (member ?b (basket))
        )
        :effect (assign (basket) (set.mk (?a ?b)))
    )
)