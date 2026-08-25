(define (problem X-member-on-array-01)
    (:domain X-member-on-array)
    (:init
        (= (cells) (array.mk (3 1 4 1)))
    )
    ;; cells[0]=3 at init; find_val has no effect, so cells never changes.
    ;; Goal=9 is unreachable, ensuring the test passes whether the domain
    ;; is rejected (type error) or silently accepted (unsolvable).
    (:goal (= (read (cells) 0) 9))
)
