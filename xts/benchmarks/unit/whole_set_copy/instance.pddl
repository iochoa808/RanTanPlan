;; original={1,2,3}, backup={}.
;; Goal: 1,2,3 ∈ backup  AND  original={}.
;; Plan: archive, flush  — 2 steps.

(define (problem archive-01)
    (:domain archive)

    (:init
        (= (original) (set.mk (1 2 3)))
        (= (backup)   (set.mk ()))
    )

    (:goal (and
        (member 1 (backup))
        (member 2 (backup))
        (member 3 (backup))
        (= (cardinality (original)) 0)
    ))
)
