(define (problem set-constant-p1)
    (:domain set-constant-test)

    ; No objects needed — using integer-typed set.

    ; Initial: active = {}, allowed = {2 4 6}
    ; Goal:    member 2 active AND member 4 active AND member 6 active
    ;
    ; Plan: activate_even(2), activate_even(4), activate_even(6)
    ;   -> cardinality(active)=3 >= cardinality(set.mk(2 4 6))=3 -> mark_complete fires
    ;
    ; Note: cherry (level 5) cannot be activated because 5 not in (set.mk (2 4 6)).
    (:init
        (= (active)  (set.mk ()))
        (= (allowed) (set.mk (2 4 6)))
    )

    (:goal
        (and
            (member 2 (active))
            (member 4 (active))
            (member 6 (active))
        )
    )
)
