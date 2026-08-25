;; cells = [0, 0, 0, 0].  All < 4, so inc_all fires immediately.
;;
;; Goal: cell(0) = 1 AND cell(3) = 1.
;;
;; Plan: inc_all()  — 1 step.
;; Verifies: the precondition forall checked all 4 indices, and the
;; effect forall applied to all 4 indices.

(define (problem forall-const-01)
    (:domain forall-const)

    (:init
        (= (cell 0) 0)
        (= (cell 1) 0)
        (= (cell 2) 0)
        (= (cell 3) 0)
    )

    (:goal (and
        (= (cell 0) 1)
        (= (cell 3) 1)
    ))
)
