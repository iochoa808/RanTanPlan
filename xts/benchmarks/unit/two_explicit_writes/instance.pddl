;; cells = [0, 0, 0, 0].
;; write_a writes 5 into cell 0; write_b then writes 7 into cell 2.
;; Cells 1 and 3 are never touched and must remain 0 (frame axiom).
;;
;; Expected plan (2 steps): write_a(), write_b()

(define (problem two-explicit-writes-01)
    (:domain two-explicit-writes)

    (:init
        (= (cells) (array.mk (0 0 0 0)))
    )

    (:goal
        (and
            (= (read (cells) (0)) 5)
            (= (read (cells) (2)) 7)
            (= (read (cells) (1)) 0)
            (= (read (cells) (3)) 0)
        )
    )
)
