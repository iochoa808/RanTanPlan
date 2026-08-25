;; cells = [3, 7, 1, 0].
;;
;; Cell 1 is already 7 > 5, so detect fires immediately.
;;
;; Goal: (found_high).
;;
;; Plan: detect()  — 1 step.
;;
;; The exists in the precondition witnesses ?i=1 (cells[1]=7 > 5).

(define (problem exists-range-01)
    (:domain exists-range)

    (:init
        (= (cells) (array.mk (3 7 1 0)))
    )

    (:goal (found_high))
)
