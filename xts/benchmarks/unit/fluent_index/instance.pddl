;; cells = [1, 3, 0].
;; Goal: cells[1] = 5.
;;
;; Plan: inc(1), inc(1) — 2 steps.

(define (problem score-counter-01)
    (:domain score-counter)

    (:init
        (= (cells) (array.mk (1 3 0)))
    )

    (:goal (= (read (cells) 1) 5))
)
