;; cells = [0, 5, 7, 0, 0].
;;
;; Goal: (prefix_done) AND (window_clear).
;;
;; Plan: zero(1), zero(2), mark_prefix(4), check_window(1, 2)  — 4 steps.
;;   After zero(1): cells = [0, 0, 7, 0, 0].
;;   After zero(2): cells = [0, 0, 0, 0, 0].
;;   mark_prefix(4): forall i in [0..4]: cells[i]=0  ✓  → prefix_done.
;;   check_window(1,2): forall i in [1..2]: cells[i]=0  ✓  → window_clear.

(define (problem forall-param-array-01)
    (:domain forall-param-array)

    (:init
        (= (cells) (array.mk (0 5 7 0 0)))
    )

    (:goal (and (prefix_done) (window_clear)))
)
