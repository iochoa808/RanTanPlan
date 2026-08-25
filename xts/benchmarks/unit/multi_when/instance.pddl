;; flag-a=true, flag-b=false, cell-a=5, cell-b=5.
;; Goal: cell-a=0, cell-b=0.
;; Plan: activate, process  — 2 steps.

(define (problem dual-gate-01)
    (:domain dual-gate)

    (:init
        (flag-a)
        (= (cell-a) 5)
        (= (cell-b) 5)
    )

    (:goal (and
        (= (cell-a) 0)
        (= (cell-b) 0)
    ))
)
