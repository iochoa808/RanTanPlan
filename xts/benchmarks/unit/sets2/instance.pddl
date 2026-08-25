(define (problem sets-showcase-p2)
    (:domain sets-showcase)

    (:objects
        a b c d e f - item
    )

    ;; shelf2 starts with {a}, which is disjoint from shelf3 = {b,c,d,e},
    ;; so bulk_fill is immediately applicable for shelf2.
    (:init
        (= (shelf1) (set.mk (a b c f)))
        (= (shelf2) (set.mk (a)))
        (= (shelf3) (set.mk (b c d e)))
    )

    ;; Two plans found by the planner (shelf1 steps run in parallel with shelf2 step):
    ;;
    ;;  shelf1 — Plan A (uses common_only + complement + add_item):
    ;;   common_only  →  shelf1 := {a,b,c} ∩ {b,c,d,e} = {b,c}   (cardinality 3 ≥ 2 ✓)
    ;;   complement   →  shelf1 := {b,c,d,e} \ {b,c}   = {d,e}   (subset {b,c}⊆{b,c,d,e} ✓)
    ;;   add_item(a)  →  shelf1 := {a,d,e}                        (a ∉ {d,e} ✓)
    ;;
    ;;  shelf1 — Plan B (uses remove_item + complement + add_item):
    ;;   remove_item(a) →  shelf1 := {b,c}
    ;;   complement     →  shelf1 := {b,c,d,e} \ {b,c} = {d,e}   (subset {b,c}⊆{b,c,d,e} ✓)
    ;;   add_item(a)    →  shelf1 := {a,d,e}
    ;;
    ;;  shelf2 — 1 step (uses bulk_fill):
    ;;   bulk_fill  →  shelf2 := {a} ∪ {b,c,d,e} = {a,b,c,d,e}  (disjoint({a},{b,c,d,e}) ✓)
    (:goal
        (and
            (= (shelf1) (set.mk (a d e f)))
            (= (shelf2) (set.mk (a b c d e)))
        )
    )
)
