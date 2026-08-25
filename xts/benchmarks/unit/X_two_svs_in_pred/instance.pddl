(define (problem X-two-svs-in-pred-01)
    (:domain X-two-svs-in-pred)
    (:init
        (= (board) (array.mk ((n0 n1 n2)
                              (n3 n4 n5)
                              (n6 n7 n8))))
        (linked n0 n1)
        (linked n4 n5)
    )
    ;; No action modifies the `linked` predicate; (linked n3 n7) is never
    ;; established. The test passes whether the domain is rejected (two-SVs
    ;; check) or silently accepted (goal unreachable → unsolvable).
    (:goal (linked n3 n7))
)
