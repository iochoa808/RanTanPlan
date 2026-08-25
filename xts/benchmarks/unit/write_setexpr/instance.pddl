;; cells = [{0,1}, {2,3}, {4}].
;; Goal: cell 2 contains 0 and 1 (merged from cell 0).
;;
;; Plan: merge_into(0, 2)  — 1 step.

(define (problem set-merger-01)
    (:domain set-merger)

    (:init
        (= (cells) (array.mk ((set.mk (0 1))
                              (set.mk (2 3))
                              (set.mk (4)))))
    )

    (:goal (and
        (member 0 (read (cells) 2))
        (member 1 (read (cells) 2))
    ))
)
