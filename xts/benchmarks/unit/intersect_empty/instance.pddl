;; a={1,3}, b={2,4} (disjoint), result={} (empty initially).
;; compute fires because disjoint(a,b) holds; sets result = intersect(a,b) = {} and (computed).
;; Goal: (computed)  — confirms the action ran; result is empty as a side-effect.
;; Plan: compute()  — 1 step.

(define (problem intersect-empty-01)
    (:domain intersect-empty)

    (:init
        (= (a) (set.mk (1 3)))
        (= (b) (set.mk (2 4)))
        (= (result) (set.mk ()))
    )

    (:goal (computed))
)
