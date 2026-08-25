;; arr = [0,0,0,0,0].
;; Goal: some cell equals 7.
;; Plan: write(0)  — 1 step (any index works).

(define (problem exists-goal-01)
    (:domain exists-goal)

    (:init
        (= (cells) (array.mk (0 0 0 0 0)))
    )

    (:goal
        (exists (?i - (number 0 4)) (= (read (cells) ?i) 7))
    )
)
