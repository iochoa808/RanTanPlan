(define (problem empty-range-forall-prob)
    (:domain empty-range-forall)

    (:init
        (= (cells) (array.mk (3 1 4 1)))
    )

    ;; done must be set AND cells[0] must be unchanged (3) — proves no-op effect
    (:goal (and
        (done)
        (= (read (cells) 0) 3)
        (= (read (cells) 2) 4)
    ))
)