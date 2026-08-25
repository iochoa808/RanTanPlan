;; cells=[3,0,0].  Goal: cells[0]=0.
;; Plan: dec(0), dec(0), dec(0)  — 3 steps.

(define (problem score-down-01)
    (:domain score-down)

    (:init
        (= (cells) (array.mk (3 0 0)))
    )

    (:goal (= (read (cells) 0) 0))
)
