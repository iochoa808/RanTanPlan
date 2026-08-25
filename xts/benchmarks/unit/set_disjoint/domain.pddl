;; Test: SET_DISJOINT operator in a precondition.
;;
;; Focused unit test for (disjoint A B): isolated from the multi-feature
;; complexity of sets2/3/4.  Uses object-type set elements (same as sets2/3)
;; because disjoint with bounded-integer element types is not supported by the
;; UP type checker (it cannot construct a typed empty set for the A∩B=∅ check).

(define (domain set-disjoint)
    (:requirements :typing :sets)

    (:types
        item    - object
        itemset - (set item)
        flag    - (number 0 1)
    )

    (:constants
        item_a item_b item_c item_d - item
    )

    (:functions
        (owned)      - itemset   ;; items this agent holds
        (restricted) - itemset   ;; items blocked by regulation
        (cleared)    - flag
    )

    ;; Proceed only if the agent holds nothing that is restricted.
    (:action proceed
        :parameters ()
        :precondition (and
            (= (cleared) 0)
            (disjoint (owned) (restricted))
        )
        :effect (assign (cleared) 1)
    )
)
