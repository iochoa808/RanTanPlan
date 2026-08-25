;; grid = [0, 0, 0, 0].
;; stamp() writes 1 to cell 0 and 2 to cell 3 in a single action.
;; Cells 1 and 2 must remain 0 (frame axiom via store-axiom propagation).
;;
;; Expected plan (1 step): stamp()

(define (problem two-writes-01)
    (:domain two-writes)

    (:init
        (= (grid) (array.mk (0 0 0 0)))
    )

    (:goal
        (and
            (= (read (grid) (0)) 1)
            (= (read (grid) (3)) 2)
            (= (read (grid) (1)) 0)
            (= (read (grid) (2)) 0)
        )
    )
)
