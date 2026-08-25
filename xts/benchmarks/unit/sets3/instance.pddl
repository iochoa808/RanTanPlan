(define (problem cold-lab-p1)
    (:domain cold-lab)

    (:objects
        a b c d e - sample
    )

    ;; archive = {a, b, c}  (disjoint from {d, e})
    ;;
    ;; Initial lab = {b, c, e}: two archive items plus one foreign item (e).
    ;; Initial temp = 4: too warm for manual load/unload, must cool down.
    ;;
    ;; Optimal parallel plan (horizon 3):
    ;;
    ;;   t=1:  trim_to_archive   →  lab  := {b,c,e} ∩ {a,b,c} = {b,c}
    ;;                                      (cardinality({b,c,e}) = 3 ≥ 3 ✓)
    ;;         cool              →  temp := 3
    ;;
    ;;   t=2:  complete_from_archive  →  lab  := {b,c} ∪ {a,b,c} = {a,b,c}
    ;;                                          ({b,c} ⊆ {a,b,c} ✓)
    ;;         cool                   →  temp := 2
    ;;
    ;;   t=3:  cool              →  temp := 1
    ;;
    ;; Goal achieved: lab = {a,b,c} ✓  temp = 1 ✓
    ;;
    ;; Note — emergency_fill alternative path (not used here):
    ;;   If lab were {d, e} (disjoint from archive), cool to temp ≤ 1, then
    ;;   emergency_fill gives {d,e} ∪ {a,b,c} = {a,b,c,d,e}, then
    ;;   trim_to_archive (cardinality 5 ≥ 3) gives {a,b,c}.
    (:init
        (= (lab)     (set.mk (b c e)))
        (= (archive) (set.mk (a b c)))
        (= (temp)    4)
    )

    (:goal
        (and
            (= (lab)  (set.mk (a b c)))
            (= (temp) 1)
        )
    )
)
