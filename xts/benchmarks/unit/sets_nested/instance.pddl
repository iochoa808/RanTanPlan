(define (problem set-nested-p1)
    (:domain set-nested-effects)

    ; Initial: bucket_a={1}, bucket_b={2}, bucket_c={3}, result={}
    ; Goal: result contains 1, 2, and 3 (all three buckets merged)
    ;
    ; Plan:
    ;   add_to_a(1), add_to_b(2), add_to_c(3), merge_all()
    ;   → result = union(union({1},{2}),{3}) = {1,2,3}
    (:init
        (= (bucket_a) (set.mk ()))
        (= (bucket_b) (set.mk ()))
        (= (bucket_c) (set.mk ()))
        (= (result)   (set.mk ()))
    )

    (:goal
        (and
            (member 1 (result))
            (member 2 (result))
            (member 3 (result))
        )
    )
)
