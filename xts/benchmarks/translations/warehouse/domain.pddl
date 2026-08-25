;; PDDL-XTS translation of pddl/test/warehouse.
;; FEATURES: array of sets + bounded integers + count.
;;   - (in ?i ?b) boolean relation + item-count numeric fluent
;;     -> array of sets (bins): bin index reads the set of items it holds.
;;   - capacity numeric fluent -> array of bounded ints.
;;   - full-bins counter, kept in sync by conditional effects
;;     -> single `count` aggregate over per-bin cardinality checks, evaluated
;;        directly in the goal (no counter fluent to maintain).

(define (domain warehouse-xts)
    (:requirements :typing :bounded-integers :arrays :sets)
    (:types
        item    - (number 0 3)
        binidx  - (number 0 2)
        cap     - (number 0 4)
        itemset - (set item)
        binsarr - (array 3 itemset)
        caparr  - (array 3 cap)
    )
    (:functions
        (bins)     - binsarr
        (capacity) - caparr
    )

    (:action move
        :parameters (?item - item ?src - binidx ?dst - binidx)
        :precondition (and
            (not (= ?src ?dst))
            (member ?item (read (bins) ?src))
            (< (cardinality (read (bins) ?dst)) (read (capacity) ?dst))
        )
        :effect (and
            (remove ?item (read (bins) ?src))
            (add ?item (read (bins) ?dst))
        )
    )
)