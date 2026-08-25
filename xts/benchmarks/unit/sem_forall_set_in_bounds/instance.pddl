;; Start with {0}. After add_top, chain contains 9.
;; Goal: 9 is a member of chain (satisfied after one add_top action).
(define (problem sem-forall-set-in-bounds-01)
    (:domain sem-forall-set-in-bounds)
    (:init
        (= (chain) (set.mk (0)))
    )
    (:goal (member 9 (chain)))
)
