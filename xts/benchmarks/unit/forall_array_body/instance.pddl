;; cells = [2, 1, 3, 1].  gate_open = false.
;;
;; Goal: cells[0] = 0 AND cells[3] = 0.
;;
;; Plan: open_gate(), reset_all()  — 2 steps.
;;   open_gate: sets gate_open.
;;   reset_all: precondition forall checks cells[0..3] > 0  ✓
;;              effect forall writes 0 to every cell.
;;
;; The precondition forall reads the array; the effect forall writes it.

(define (problem forall-array-body-01)
    (:domain forall-array-body)

    (:init
        (= (cells) (array.mk (2 1 3 1)))
    )

    (:goal (and
        (= (read (cells) 0) 0)
        (= (read (cells) 3) 0)
    ))
)
