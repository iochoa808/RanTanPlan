;; Test: assign a set fluent to a set.mk LITERAL in an action effect.
;;
;; KEY PATTERN: (assign (basket) (set.mk (a b)))
;; set.mk with constant object elements appears in action effects, not just init.
;; (Previously only (set.mk) empty-clear was tested in effects; (assign f (union ...))
;; was tested, but assigning a fresh constant-element set was not.)
;;
;; Domain: a basket of items; a reset action replaces the basket with a fixed pair.

(define (domain reset-basket)
    (:requirements :sets)

    (:types
        item    - object
        itemset - (set item)
    )

    (:constants item_a item_b item_c item_d - item)

    (:functions
        (basket) - itemset
    )

    ;; Add any single item.
    (:action add_item
        :parameters (?x - item)
        :precondition (not (member ?x (basket)))
        :effect (add ?x (basket))
    )

    ;; When basket has 3+ items, reset it to exactly {item_a, item_b}.
    (:action reset_to_ab
        :parameters ()
        :precondition (>= (cardinality (basket)) 3)
        :effect (assign (basket) (set.mk (item_a item_b)))
    )
)
