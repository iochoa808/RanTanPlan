(define (problem X-forall-range-exceeds-array-01)
    (:domain X-forall-range-exceeds-array)
    (:init
        (= (cells) (array.mk (5 3 8 1)))
    )
    ;; zero_all writes 0 to every cell.  cells[0] starts at 5 but becomes 0,
    ;; not 7.  Goal=7 is unreachable regardless of OOB behaviour.
    (:goal (= (read (cells) 0) 7))
)
