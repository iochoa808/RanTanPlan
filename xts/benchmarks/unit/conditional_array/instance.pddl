;; chips: ok_chip (live), dead_chip (not live).
;; cells = [0, 0, 0].
;;
;; Goal: cells[1] = 3.
;;
;; Plan: activate(ok_chip, 1)  — 1 step.
;; activate(dead_chip, 1) would pass the precondition but the when-body
;; would not fire, leaving cells[1] = 0.

(define (problem cond-array-01)
    (:domain cond-array)

    (:objects
        ok_chip dead_chip - chip
    )

    (:init
        (= (cells) (array.mk (0 0 0)))
        (live ok_chip)
    )

    (:goal (= (read (cells) 1) 3))
)
