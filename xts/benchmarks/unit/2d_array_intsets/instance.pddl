;; codes row 0 = [{1,2}, {3}, {4,0}].
;; codes row 1 = [{0}, {0}, {0}].
;; active_c = {0, 2}  (copy only columns 0 and 2).
;;
;; Goal: row-1 col-0 contains code 1; row-1 col-2 contains code 4.
;;
;; Plan: propagate(0), propagate(2)  — 2 steps.

(define (problem code-grid-01)
    (:domain code-grid)

    (:init
        (= (codes)    (array.mk (((set.mk (1 2)) (set.mk (3)) (set.mk (4 0)))
                                 ((set.mk (0))   (set.mk (0)) (set.mk (0))))))
        (= (active_c) (set.mk (0 2)))
    )

    (:goal (and
        (member 1 (read (codes) 1 0))
        (member 4 (read (codes) 1 2))
    ))
)
