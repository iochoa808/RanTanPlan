(define (domain sets-showcase)

    (:requirements :sets)

    (:types
        item   - object
        itemset - (set item)   ;; set type: itemset is a "set of item"
    )

    (:functions
        (shelf1) - itemset   ;; working set A — modified by shelf1 actions
        (shelf2) - itemset   ;; working set B — filled by bulk_fill
        (shelf3) - itemset   ;; reference set  — read-only
    )

    ;; ── shelf1 actions ─────────────────────────────────────────────────────────

    ;; Feature: member (precondition), add (effect)
    (:action add_item
        :parameters (?x - item)
        :precondition (not (member ?x (shelf1)))
        :effect (add ?x (shelf1))
    )

    ;; Feature: member (precondition), remove (effect)
    (:action remove_item
        :parameters (?x - item)
        :precondition (member ?x (shelf1))
        :effect (remove ?x (shelf1))
    )

    ;; Feature: disjoint (precondition), union (effect)
    ;; Merge shelf3 into shelf1 when the two sets share no items.
    (:action bulk_add
        :parameters ()
        :precondition (disjoint (shelf1) (shelf3))
        :effect (assign (shelf1) (union (shelf1) (shelf3)))
    )

    ;; Feature: cardinality (precondition), intersect (effect)
    ;; Keep only items common to shelf1 and shelf3; requires shelf1 to have >=2 items.
    (:action common_only
        :parameters ()
        :precondition (>= (cardinality (shelf1)) 2)
        :effect (assign (shelf1) (intersect (shelf1) (shelf3)))
    )

    ;; Feature: subset (precondition), difference (effect)
    ;; When shelf1 ⊆ shelf3, flip shelf1 to the items in shelf3 not already in shelf1.
    (:action complement
        :parameters ()
        :precondition (subset (shelf1) (shelf3))
        :effect (assign (shelf1) (difference (shelf3) (shelf1)))
    )

    ;; ── shelf2 actions ─────────────────────────────────────────────────────────

    ;; Feature: disjoint (precondition), union (effect)
    ;; Fill shelf2 with everything in shelf3 when the two sets share no items.
    (:action bulk_fill
        :parameters ()
        :precondition (disjoint (shelf2) (shelf3))
        :effect (assign (shelf2) (union (shelf2) (shelf3)))
    )
)
